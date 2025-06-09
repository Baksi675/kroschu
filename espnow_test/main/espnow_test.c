#include <stdbool.h>
#define RECEIVER 0
#define SENDER 1

#if SENDER

#include "esp_err.h"			// Required for ESP_ERROR_CHECK()
#include "esp_event.h"			// Required for event driver programming (esp_event_... functions)
#include "esp_wifi.h"			// Required for the Wi-Fi
#include "nvs_flash.h"			// Required for nvs_flash_init()
#include "esp_netif.h"			// Required for esp_netif_init()
#include "esp_now.h"			// Required for ESP-NOW
#include "string.h"				// Required for memcpy
#include "esp_log.h"

	// INFO: Max number of paired devices is 20, with encryption it is 17. Default number of devices is set to 7, this can be changed with CONFIG_ESP_WIFI_ESPNOW_MAX_ENCRYPT_NUM
	
	// 1. Enable WiFi ==> Recommended before initializing ESP-NOW
	// 2. esp_now_init() ==> Initializes ESP-NOW
	// 3. esp_now_add_peer() ==> Add the device to the paired device list, before sending data to it
	// 4. If security is enabled LMK must be set
	// 5. Station or SoftAP interface must be initialized before sending data with ESP-NOW
	// 6. Set channel (range 0-14), 0 - current channel, choose the one with the least amount of load

static const char *TAG = "ESP-NOW Sender";
static bool canSend = true;

static void espnow_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
{
    //ESP_LOGI(TAG, "Send status: %s", (status == ESP_NOW_SEND_SUCCESS) ? "Success" : "Fail");
    canSend = true;
}


void app_main(void)
{
	ESP_ERROR_CHECK(nvs_flash_init());			// ESP_ERROR_CHECK ==> returns ESP_OK if no error happened, otherwise prints error message
												// nvs_flash_init() ==> Initializes the default NVS (Non Volatile Storage) partition, 
												// 		required for many components (Wi-Fi, Bluetooth, ESP-NOW) to store and retrieve settings in non-volatile storage
	ESP_ERROR_CHECK(esp_netif_init());			// 	esp_netif_init() ==> It's used to initialize the networking stack (NETIF). Required for network interfaces (STA, AP)		
	
	ESP_ERROR_CHECK(esp_event_loop_create_default());	// esp_event_loop_create_default() ==> Handles system events (eg. Wi-Fi connection events, ESP-NOW events).
														//		Internally used by Wi-Fi and ESP-NOW components (that's why required)
														//		Can also be used to handle events like Wi-Fi disconnected, package sent etc.
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();	// wifi_init_config_t ==> A structure that contains the initialization values for the Wi-Fi driver
															//		WIFI_INIT_CONFIG_DEFAULT() ==> Assign the default values to the struct (only needs to be other when doing advanced tuning)
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));			// esp_wifi_init() ==> Initializes the Wi-Fi driver (with the cfg structure passed as parameter)
	
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));	// esp_wifi_set_mode() ==> Sets the Wi-Fi mode, in this case to STATION (acts as a client, connects to a Wi-Fi network)

	ESP_ERROR_CHECK(esp_wifi_start());	// esp_wifi_start() ==> Starts the Wi-Fi driver

	ESP_ERROR_CHECK(esp_now_init());	// esp_now_init() ==> Initializes the ESP-NOW

	esp_now_peer_info_t peer = {		// A struct that contains the data about the other end of the communication (the settings of the communication)
		.channel = 0,					//	current channel (the channel that was last set)
		.ifidx = ESP_IF_WIFI_STA,		// 	interface is STA mode
		.encrypt = false				// 	no encryption
	};
	memcpy(peer.peer_addr, "\xFF\xFF\xFF\xFF\xFF\xFF", 6);	// memcpy ==> Fills in the peer_addr field with the broadcast MAC address
	ESP_ERROR_CHECK(esp_now_add_peer(&peer));			// esp_now_add_peer ==> Adds the peer to the peer list, only required when using unicast or multiple unicast (multicast is not natively present)

	ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));

	/*uint8_t data[] = "hello espnow";	// The message to be sent
	ESP_ERROR_CHECK(esp_now_send(peer.peer_addr, data, sizeof(data)));*/	// esp_now_send ==> Sends the data to the peer

	uint8_t data[1000];	// The message to be sent
	memset(data, 'A', sizeof(data));

	//ESP_ERROR_CHECK(esp_now_send(peer.peer_addr, data, sizeof(data)));

	while(1) {	
		if (canSend) {
			ESP_ERROR_CHECK(esp_now_send(peer.peer_addr, data, sizeof(data)));
			canSend = false;
		}
		vTaskDelay(pdMS_TO_TICKS(5));  // Yield CPU
	}
}

#endif

/*
#if RECEIVER

#include <string.h>
#include <stdio.h>
#include <sys/time.h>   // For gettimeofday
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

static const char *TAG = "ESP-NOW Receiver";

static uint64_t total_bytes = 0;
static struct timeval start_time;
static bool timing_started = false;

// Helper function to get elapsed time in seconds (float)
static float elapsed_time_seconds() {
    struct timeval now;
    gettimeofday(&now, NULL);
    uint64_t elapsed_us = (now.tv_sec - start_time.tv_sec) * 1000000ULL + (now.tv_usec - start_time.tv_usec);
    return elapsed_us / 1e6f;
}

// Callback function when data is received
void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (!timing_started) {
        gettimeofday(&start_time, NULL);
        timing_started = true;
        total_bytes = 0;
    }

    total_bytes += len;

    // Optional: print info about this packet
    // char mac_str[18];
    // snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
    //          recv_info->src_addr[0], recv_info->src_addr[1], recv_info->src_addr[2],
    //          recv_info->src_addr[3], recv_info->src_addr[4], recv_info->src_addr[5]);
    // ESP_LOGI(TAG, "Received %d bytes from %s", len, mac_str);
}

void app_main(void) {
    // Initialize NVS and Wi-Fi
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Initialize ESP-NOW
    ESP_ERROR_CHECK(esp_now_init());

    // Register receive callback
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));

    ESP_LOGI(TAG, "Ready to receive ESP-NOW data.");

    // Periodically print throughput every 1 second
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (timing_started) {
            float seconds = elapsed_time_seconds();
            float kbps = (total_bytes * 8) / (seconds * 1000);  // kilobits per second
            ESP_LOGI(TAG, "Received %llu bytes in %.2f seconds, Speed: %.2f kbps", total_bytes, seconds, kbps);
        } else {
            ESP_LOGI(TAG, "No data received yet...");
        }
    }
}*/

#if RECEIVER

#include <string.h>
#include <stdio.h>
#include <sys/time.h>   // For gettimeofday
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

static const char *TAG = "ESP-NOW Receiver";

static uint64_t total_bytes = 0;
static struct timeval start_time;
static bool timing_started = false;

// Helper function to get elapsed time in seconds (float)
static float elapsed_time_seconds() {
    struct timeval now;
    gettimeofday(&now, NULL);
    uint64_t elapsed_us = (now.tv_sec - start_time.tv_sec) * 1000000ULL + (now.tv_usec - start_time.tv_usec);
    return elapsed_us / 1e6f;
}

// Callback function when data is received
void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (!timing_started) {
        gettimeofday(&start_time, NULL);
        timing_started = true;
        total_bytes = 0;
    }

    total_bytes += len;

    // Optional: print info about this packet
    // char mac_str[18];
    // snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
    //          recv_info->src_addr[0], recv_info->src_addr[1], recv_info->src_addr[2],
    //          recv_info->src_addr[3], recv_info->src_addr[4], recv_info->src_addr[5]);
    // ESP_LOGI(TAG, "Received %d bytes from %s", len, mac_str);
}

void app_main(void) {
    // Initialize NVS and Wi-Fi
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Initialize ESP-NOW
    ESP_ERROR_CHECK(esp_now_init());

    // Register receive callback
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));

    ESP_LOGI(TAG, "Ready to receive ESP-NOW data.");

    // Periodically print throughput every 1 second
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (timing_started) {
            float seconds = elapsed_time_seconds();
            float kbps = (total_bytes * 8) / (seconds * 1000);  // kilobits per second
            ESP_LOGI(TAG, "Received %llu bytes in %.2f seconds, Speed: %.2f kbps", total_bytes, seconds, kbps);
        } else {
            ESP_LOGI(TAG, "No data received yet...");
        }
    }
}

#endif

