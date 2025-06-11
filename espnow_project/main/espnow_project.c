#include "esp_log_args.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"

#define SENDER 1
#define RECEIVER 0

#include "esp_err.h"			// Required for ESP_ERROR_CHECK()
#include "esp_event.h"			// Required for event driver programming (esp_event_... functions)
#include "esp_wifi.h"			// Required for the Wi-Fi
#include "nvs_flash.h"			// Required for nvs_flash_init()
#include "esp_netif.h"			// Required for esp_netif_init()
#include "esp_now.h"			// Required for ESP-NOW
#include "string.h"				// Required for memcpy
#include "esp_log.h"

#if SENDER

/********** HUB **********/


#define TAG "ESP_SENDER"

bool canSend = true;

void CommsInit(void);
void PeersInit(void);
static void espnow_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status);

#define RECEIVER_NUM 2

typedef struct{
	uint8_t macAddr[6];
}mac_t;

mac_t receiverMacArr[] = {
	{{0xF0, 0xF5, 0xBD, 0x01, 0xB3, 0x48}},
	{{0x94, 0xA9, 0x90, 0x03, 0x8F, 0x48}}
};

esp_now_peer_info_t peersArr[RECEIVER_NUM];

void app_main(void)
{
	CommsInit();
	PeersInit();

	/*esp_now_peer_info_t peer = {		
		.channel = 0,					
		.ifidx = ESP_IF_WIFI_STA,		
		.encrypt = false			
	};

	uint8_t receiver_mac[] = {0xF0, 0xF5, 0xBD, 0x01, 0xB3, 0x48};*/
	//uint8_t receiver_mac[] = {0x54, 0x32, 0x04, 0x07, 0x41, 0xF4};

	//uint8_t receiver_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

	//memcpy(peer.peer_addr, receiver_mac, 6);	
	
	//ESP_ERROR_CHECK(esp_now_add_peer(&peer));		



	/*esp_now_peer_info_t peer2 = {		
		.channel = 0,					
		.ifidx = ESP_IF_WIFI_STA,		
		.encrypt = false				
	};

	uint8_t receiver_mac2[] = {0x54, 0x32, 0x04, 0x07, 0x42, 0xC0};

	memcpy(peer2.peer_addr, receiver_mac2, 6);	

	ESP_ERROR_CHECK(esp_now_add_peer(&peer2));		*/

	


	ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));

	uint8_t data[ESP_NOW_MAX_DATA_LEN_V2];
	memset(data, 'A', sizeof(data));

	uint16_t nextPeer = 0;

	while(1) {
		//__asm__ volatile("NOP");
		if(canSend) {
			canSend = false;
			ESP_ERROR_CHECK(esp_now_send(peersArr[nextPeer].peer_addr, data, sizeof(data)));
			nextPeer++;
			nextPeer = nextPeer % RECEIVER_NUM;
			vTaskDelay(pdMS_TO_TICKS(10));
		}
	}
}

void CommsInit(void) {
	ESP_ERROR_CHECK(nvs_flash_init());										
												
	ESP_ERROR_CHECK(esp_netif_init());			
	
	ESP_ERROR_CHECK(esp_event_loop_create_default());	
																									
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();	
															
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));			
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));	
	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_ERROR_CHECK(esp_now_init());	
}

void PeersInit(void) {
	for(uint16_t i = 0; i < RECEIVER_NUM; i++) {

		esp_now_peer_info_t peer = {		
		.channel = 0,					
		.ifidx = ESP_IF_WIFI_STA,		
		.encrypt = false
		};

		memcpy(peer.peer_addr, receiverMacArr[i].macAddr, 6);
		
		ESP_ERROR_CHECK(esp_now_add_peer(&peer));	

		peersArr[i] = peer; 
	}
}

static void espnow_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
{
    //ESP_LOGI(TAG, "Send status: %s", (status == ESP_NOW_SEND_SUCCESS) ? "Success" : "Fail");
    canSend = true;
	//ESP_LOGI(TAG, "Message sent");
}

#endif

#if RECEIVER

/********** STATION **********/

static const char *TAG = "ESP-NOW Receiver";

#define SPEED_MEASUREMENT_PRIORITY 3

void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len);

void CommsInit(void);
void PrintMacAddress(void);
void MeasureSpeed(void *arg);

float numBitsRecv = 0;			// Number of bytes received
SemaphoreHandle_t semaphoreNumBitsRecv;

void app_main(void) {
	CommsInit();

	PrintMacAddress();

	semaphoreNumBitsRecv = xSemaphoreCreateMutex();

	xTaskCreatePinnedToCore(MeasureSpeed, "Speed measurement task", 4096, NULL, SPEED_MEASUREMENT_PRIORITY, NULL, tskNO_AFFINITY);
}

void CommsInit(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_now_init());

    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
}

void PrintMacAddress(void) {
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(ESP_IF_WIFI_STA, mac));
    ESP_LOGI("MAC_ADDRESS", "Receiver MAC: %02X:%02X:%02X:%02X:%02X:%02X", 
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void MeasureSpeed(void *arg) {
	while(1) {
		xSemaphoreTake(semaphoreNumBitsRecv, pdMS_TO_TICKS(10));
		ESP_LOGI(TAG, "Speed: %.2f kBits/s", numBitsRecv / 1000);
		numBitsRecv = 0;
		xSemaphoreGive(semaphoreNumBitsRecv);

		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    // Optional: print info about this packet
    // char mac_str[18];
    // snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
    //          recv_info->src_addr[0], recv_info->src_addr[1], recv_info->src_addr[2],
    //          recv_info->src_addr[3], recv_info->src_addr[4], recv_info->src_addr[5]);
    // ESP_LOGI(TAG, "Received %d bytes from %s", len, mac_str);

	//uint8_t const *pData = data;

	/*for(uint32_t i = 0; i < len; i++) {
		printf("%c", *pData);
		pData++;
	}*/
	//ESP_LOG_BUFFER_CHAR(TAG, data, len); 

	xSemaphoreTake(semaphoreNumBitsRecv, pdMS_TO_TICKS(10));
	numBitsRecv += len * 8;
	xSemaphoreGive(semaphoreNumBitsRecv);
}

#endif