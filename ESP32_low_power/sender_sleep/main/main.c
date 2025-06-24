// sender_sleep: main.c
// This example demonstrates how to use ESP-NOW to send a small data packet
// and then enter deep sleep for power saving. It tracks boot count across
// wakeups and timestamps each transmission.

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_sleep.h"
#include "esp_timer.h"

// Logging tag used by ESP_LOGx macros
static const char *TAG = "espnow_sleep";

// Duration for deep sleep (in seconds)
#define TIME_TO_SLEEP   30
// Conversion factor from seconds to microseconds
#define USEC_TO_SEC_FACTOR 1000000ULL

// MAC address of the receiver peer (update to match your receiver)
static uint8_t peerAddress[6] = { 0xF0, 0xF5, 0xBD, 0x01, 0xB3, 0x48 };

// Message payload structure
typedef struct {
    int           bootCount;    // Number of times device has booted
    unsigned long timestamp;    // Timestamp in milliseconds
} struct_message;

// Persist boot count across deep sleep cycles
RTC_DATA_ATTR static int bootCount = 0;

// Print the reason for the last wakeup
static void print_wakeup_reason(void)
{
    esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();
    switch (reason) {
        case ESP_SLEEP_WAKEUP_TIMER:
            ESP_LOGI(TAG, "Wakeup reason: Timer");
            break;
        case ESP_SLEEP_WAKEUP_EXT0:
            ESP_LOGI(TAG, "Wakeup reason: External signal (EXT0)");
            break;
        case ESP_SLEEP_WAKEUP_EXT1:
            ESP_LOGI(TAG, "Wakeup reason: External signal (EXT1)");
            break;
        default:
            ESP_LOGI(TAG, "Wakeup reason: Other (%d)", reason);
            break;
    }
}

// Callback invoked when an ESP-NOW packet is sent
static void on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    ESP_LOGI(TAG, "Last packet send status: %s",
             (status == ESP_NOW_SEND_SUCCESS) ? "Success" : "Fail");
}

void app_main(void)
{
    esp_err_t ret;

    // 1) Initialize non-volatile storage (NVS) for Wi-Fi and ESP-NOW
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Reduce Wi-Fi logging noise
    esp_log_level_set("wifi", ESP_LOG_ERROR);

    // 2) Initialize TCP/IP network interface and default event loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 3) Configure Wi-Fi in Station mode (required for ESP-NOW)
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // 4) Increment and log boot count
    bootCount++;
    ESP_LOGI(TAG, "Boot count: %d", bootCount);

    // 5) Report the wakeup source
    print_wakeup_reason();

    // 6) Initialize ESP-NOW and register send callback
    ret = esp_now_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW initialization failed: %d", ret);
    } else {
        ESP_ERROR_CHECK(esp_now_register_send_cb(on_data_sent));

        // Set up peer information
        esp_now_peer_info_t peer = { 0 };
        memcpy(peer.peer_addr, peerAddress, 6);
        peer.channel = 0;         // Use current Wi-Fi channel
        peer.encrypt = false;     // Disable encryption

        // Add peer to peer list
        if (esp_now_add_peer(&peer) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to add ESP-NOW peer");
        } else {
            // Prepare message payload
            struct_message msg = {
                .bootCount = bootCount,
                .timestamp = (unsigned long)(esp_timer_get_time() / 1000)
            };

            // Send the message
            ret = esp_now_send(peerAddress, (uint8_t *)&msg, sizeof(msg));
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "ESP-NOW send error: %d", ret);
            }
            // Optional: wait for callback confirmation
            // vTaskDelay(pdMS_TO_TICKS(200));
        }
    }

    // 7) Enter deep sleep to conserve power
    ESP_LOGI(TAG, "Entering deep sleep for %d seconds", TIME_TO_SLEEP);
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * USEC_TO_SEC_FACTOR);
    ESP_ERROR_CHECK(esp_wifi_stop());  // Turn off Wi-Fi before sleeping
    esp_deep_sleep_start();
}
