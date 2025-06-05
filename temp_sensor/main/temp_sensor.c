/*DHT22*/

/*
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "rom/ets_sys.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_wifi.h"
#include "esp_event.h"


#include "DHT22.h"

oid DHT_reader_task(void *pvParameter)
{
		setDHTgpio(GPIO_NUM_0);
		

	while(1) {
	
		printf("DHT Sensor Readings\n" );
		int ret = readDHT();
		
		errorHandler(ret);

		printf("Humidity %.2f %%\n", getHumidity());
		printf("Temperature %.2f degC\n\n", getTemperature());
		
		vTaskDelay(pdMS_TO_TICKS(2000));
	}
}

void app_main()
{
	nvs_flash_init();
    vTaskDelay(pdMS_TO_TICKS(2000));
	xTaskCreate(&DHT_reader_task, "DHT_reader_task", 2048, NULL, 5, NULL );
}*/

/*
	SENDER2
*/
/*
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "espnow_example";
char msg2[1000];

void CreateMessage(void);

// Callback when data is sent
void espnow_send_cb(const wifi_tx_info_t *info, esp_now_send_status_t status)
{
    if (info) {
        printf("Send callback, MAC");
    } else {
        printf("Send callback, info is NULL, Status: %d\n", status);
    }
}

void app_main(void)
{
    // Initialize NVS (required for Wi-Fi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize TCP/IP stack and event loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize Wi-Fi in STA mode (required for ESP-NOW)
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Init ESP-NOW
    ESP_ERROR_CHECK(esp_now_init());

    // Register send callback
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));

    // Add broadcast peer (to send to all devices)
    esp_now_peer_info_t peerInfo = {0};
    memset(peerInfo.peer_addr, 0xFF, 6); // Broadcast MAC address
    peerInfo.channel = 0;  // use current channel
    peerInfo.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peerInfo));

    // Message to send
    const char *msg = "Hello ESP-NOW!";

	CreateMessage();

    while (true) {
        ESP_ERROR_CHECK(esp_now_send(peerInfo.peer_addr, (const uint8_t *)msg2, sizeof(msg2)));
        ESP_LOGI(TAG, "Sent: %s", msg);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void CreateMessage(void) {
 	memset(msg2, 'A', sizeof(msg2));
}
*/



/*
	RECEIVER2
*/
/*
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

void print_mac_address();

static const char *TAG = "espnow_receiver";
uint32_t receivedCounter, correctCounter = 0;

// Callback when data is received
void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len)
{
    if (recv_info == NULL || data == NULL || data_len <= 0) {
        ESP_LOGW(TAG, "Invalid receive callback params");
        return;
    }

    const uint8_t *mac = recv_info->src_addr;

    ESP_LOGI(TAG, "Received data from: %02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    correctCounter = 0;
	receivedCounter = 0;
    ESP_LOGI(TAG, "Data (%d bytes):", data_len);
    for (int i = 0; i < data_len; i++) {
        printf("%02x ", data[i]);
		if(data[i] == 'A') {
			correctCounter++;
		}
		receivedCounter++;
    }
	printf("Total messages received: %lu\n", receivedCounter);
	printf("Correct messages received: %lu\n", correctCounter);
    printf("\n");
}

void app_main(void)
{
    // Initialize NVS (required for Wi-Fi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize TCP/IP stack and event loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize Wi-Fi in STA mode (required for ESP-NOW)
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Init ESP-NOW
    ESP_ERROR_CHECK(esp_now_init());

    // Register receive callback
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));

    ESP_LOGI(TAG, "ESP-NOW Receiver Initialized");
    // Receiver task can be empty since callbacks handle incoming messages
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
		print_mac_address();
    }
}
*/



/*******************************BEGIN: PURPOSE OF MODULE*******************************/
/*
--> This module is responsible for sending out a broadcast message upon initialization. 
--> The broadcast message contains the masters MAC address (this only happens on initialization).
--> After this a handshake can happen between the master and the slaves.
--> The master collects the temperature and humidity data from the slaves and exposes this in an API.
*/
/*******************************END: PURPOSE OF MODULE*******************************/



/*******************************BEGIN: MODULE HEADER FILE INCLUDE*******************************/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
/*******************************END: MODULE HEADER FILE INCLUDE*******************************/



/*******************************BEGIN: GLOBAL VARIABLES PRIVATE TO MODULE*******************************/

static const char *masterMacAddr = "";
static uint8_t mac[6];

/*******************************END: GLOBAL VARIABLES PRIVATE TO MODULE*******************************/



/*******************************BEGIN: HELPER FUNCTION PROTOTYPES PRIVATE TO MODULE*******************************/
 void GetMacAddr(void);
 void MasterSendBroadcast();
 void espnow_send_cb(const wifi_tx_info_t *info, esp_now_send_status_t status);
/*******************************END: HELPER FUNCTION PROTOTYPES PRIVATE TO MODULE*******************************/



/*******************************BEGIN: APIs EXPOSED BY THIS MODULE*******************************/

/*******************************API INFORMATION*******************************
 * @fn		- 
 * 
 * @brief	- 
 * 
 * @param[in]	- 
 * @param[in]	- 
 * @param[in]	- 
 * 
 * @return	- 
 * 
 * @note	- 
 *****************************************************************************/
void MasterInit(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));

    // Add broadcast peer
    uint8_t broadcast_addr[6];
    memset(broadcast_addr, 0xFF, 6);
    esp_now_peer_info_t peerInfo = {0};
    memcpy(peerInfo.peer_addr, broadcast_addr, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    ESP_ERROR_CHECK(esp_now_add_peer(&peerInfo));
}



/*******************************API INFORMATION*******************************
 * @fn		- 
 * 
 * @brief	- 
 * 
 * @param[in]	- 
 * @param[in]	- 
 * @param[in]	- 
 * 
 * @return	- 
 * 
 * @note	- 
 *****************************************************************************/
void MasterSendBroadcast(void) {
    uint8_t broadcast_addr[6];
    memset(broadcast_addr, 0xFF, 6);

    esp_err_t send_result = esp_now_send(broadcast_addr, mac, sizeof(mac));
    if (send_result == ESP_OK) {
        ESP_LOGI("MASTER", "Broadcasted MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } else {
        ESP_LOGE("MASTER", "Failed to broadcast MAC, error: %s", esp_err_to_name(send_result));
    }
}


/*******************************END: APIs EXPOSED BY THIS MODULE*******************************/
 
 
 
/*******************************BEGIN: HELPER FUNCTION DEFINITIONS*******************************/

/*******************************FUNCTION INFORMATION*******************************
 * @fn		- 
 * 
 * @brief	- 
 * 
 * @param[in]	- 
 * @param[in]	- 
 * @param[in]	- 
 * 
 * @return	- 
 * 
 * @note	- 
 *****************************************************************************/
 void GetMacAddr(void) {
    esp_err_t err = esp_wifi_get_mac(WIFI_IF_STA, mac);  // Get MAC for STA interface
    if (err == ESP_OK) {
        printf("MAC Address (STA): %02X:%02X:%02X:%02X:%02X:%02X\n",
               mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    } else {
        printf("Failed to get MAC address, error: %d\n", err);
    }
 }

 // Callback when data is sent
void espnow_send_cb(const wifi_tx_info_t *info, esp_now_send_status_t status)
{
    if (info) {
        printf("Send callback, MAC");
    } else {
        printf("Send callback, info is NULL, Status: %d\n", status);
    }
}
 /*******************************END: HELPER FUNCTION DEFINITIONS*******************************/

 void app_main(void) {
	MasterInit();
	GetMacAddr();
	MasterSendBroadcast();

	while(1) {
	//	GetMacAddr();
	//	vTaskDelay(pdMS_TO_TICKS(1000));
	}
 }
