// peer: main.c
// This example shows how to receive data via ESP-NOW
// and optionally filter by a specific sender MAC address.

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Tag for ESP-IDF logging
static const char *TAG = "espnow_recv";

// MAC address of the sender to filter (broadcast address for all)
static uint8_t senderAddress[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

// Structure of the received message, matching sender payload
typedef struct {
    int           bootCount;    // Boot counter from sender
    unsigned long timestamp;    // Timestamp in milliseconds
} struct_message;

// Callback invoked when an ESP-NOW packet is received
static void on_data_recv(const esp_now_recv_info_t *info,
                         const uint8_t *data, int len)
{
    struct_message msg;
    // Verify data length
    if (len != sizeof(msg)) {
        ESP_LOGW(TAG, "Unexpected data length %d (expected %d)", len, sizeof(msg));
        return;
    }
    // Copy payload into struct
    memcpy(&msg, data, sizeof(msg));

    // Format sender MAC address string
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             info->src_addr[0], info->src_addr[1], info->src_addr[2],
             info->src_addr[3], info->src_addr[4], info->src_addr[5]);

    // Log received data
    ESP_LOGI(TAG, "Received from %s → bootCount=%d, timestamp=%lums",
             mac_str, msg.bootCount, msg.timestamp);
}

void app_main(void)
{
    esp_err_t ret;

    // 1) Initialize NVS flash (required for Wi-Fi and ESP-NOW)
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Suppress Wi-Fi driver logs below ERROR
    esp_log_level_set("wifi", ESP_LOG_ERROR);

    // 2) Initialize TCP/IP stack and default event loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 3) Configure Wi-Fi in Station mode (required for ESP-NOW)
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // 4) Enable power-saving modem sleep between packets
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_MIN_MODEM));
    ESP_LOGI(TAG, "Modem-sleep enabled");

    // 5) Initialize ESP-NOW and register receive callback
    ret = esp_now_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW init failed: %d", ret);
        return;
    }
    ESP_LOGI(TAG, "ESP-NOW initialized");
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_data_recv));

    // 6) (Optional) Filter by specific peer
    esp_now_peer_info_t peer = { 0 };
    memcpy(peer.peer_addr, senderAddress, 6);
    peer.channel = 0;         // Use current Wi-Fi channel
    peer.encrypt = false;     // No encryption
    if (esp_now_add_peer(&peer) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to add peer filter");
    }

    // 7) Loop forever to keep receiving callbacks
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}