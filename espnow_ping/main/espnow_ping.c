/*
	To send data to devices: 
	--> Set the number of receivers (RECEIVER_NUM)
	--> Set their MAC addresses (receiverMacArr[])
*/

#include "esp_log_buffer.h"
#include "hal/gpio_types.h"
#include "portmacro.h"
#include "soc/gpio_num.h"
#define SENDER 0
#define RECEIVER 1

//#include "esp_log_args.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "esp_err.h"			// Required for ESP_ERROR_CHECK()
#include "esp_event.h"			// Required for event driver programming (esp_event_... functions)
#include "esp_wifi.h"			// Required for the Wi-Fi
#include "nvs_flash.h"			// Required for nvs_flash_init()
#include "esp_netif.h"			// Required for esp_netif_init()
#include "esp_now.h"			// Required for ESP-NOW
#include "string.h"				// Required for memcpy
#include "esp_log.h"
#include <stdint.h>
#include "esp_timer.h"
#include "driver/gpio.h"

#if RECEIVER

/********** HUB **********/


#define TAG "ESP_SENDER"

uint8_t cmd[] = {0xFF};
uint8_t ack[] = {0xFA};

bool canReceiveData = false;
bool canSendCmd = false;

void CommsInit(void);
void PeersInit(void);
void SendCmd(void);

static void espnow_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status);
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len);

#define RECEIVER_NUM 1

typedef struct{
	uint8_t macAddr[6];
}mac_t;

mac_t senderMacAddr[] = {
	{{0xF0, 0xF5, 0xBD, 0x01, 0xB3, 0x48}}
};

esp_now_peer_info_t peersArr[RECEIVER_NUM];

TickType_t start, end;

void app_main(void)
{
	CommsInit();
	PeersInit();

	ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
	ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));

	uint8_t data[ESP_NOW_MAX_DATA_LEN_V2];
	memset(data, 'A', sizeof(data));


	canSendCmd = true;

	gpio_set_direction(GPIO_NUM_2,  GPIO_MODE_OUTPUT);
	gpio_set_level(GPIO_NUM_2, 1);

	while(1) {

		if(canSendCmd) {
			SendCmd();
		}

		vTaskDelay(pdMS_TO_TICKS(1000));
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

	ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

	ESP_ERROR_CHECK(esp_now_init());	
}

void PeersInit(void) {
	for(uint16_t i = 0; i < RECEIVER_NUM; i++) {

		esp_now_peer_info_t peer = {		
			.channel = 0,					
			.ifidx = ESP_IF_WIFI_STA,		
			.encrypt = false
		};

		memcpy(peer.peer_addr, senderMacAddr[i].macAddr, 6);
		
		ESP_ERROR_CHECK(esp_now_add_peer(&peer));	

		peersArr[i] = peer; 
	}
}

void SendCmd(void) {
	start = esp_timer_get_time();
	ESP_ERROR_CHECK(esp_now_send(peersArr[0].peer_addr, cmd, sizeof(cmd)));
	gpio_set_level(GPIO_NUM_2, 1);
}

static void espnow_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
{
	// CMD
	if(canSendCmd) {
		ESP_LOGI(TAG, "CMD sent");
		canSendCmd = false;
	}
}

static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {

	if(canReceiveData) {
		gpio_set_level(GPIO_NUM_2, 0);
		end = esp_timer_get_time();
		ESP_LOGI(TAG, "Data received: ");
		ESP_LOG_BUFFER_HEX(TAG, data, len);
		float duration_ms = end - start;
		ESP_LOGI(TAG, "Time: %.4f us", duration_ms);
		canReceiveData = false;
		canSendCmd = true;
	}

	// ACK
	if(*data == 0xFA) {
		ESP_LOGI(TAG, "ACK received");
		canReceiveData = true;
	}
}

#endif

#if SENDER

/********** STATION **********/

static const char *TAG = "ESP-NOW Receiver";

#define SPEED_MEASUREMENT_PRIORITY 3

void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len);
static void espnow_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status);

void CommsInit(void);
void PrintMacAddress(void);
void MeasureSpeed(void *arg);
void PeersInit(void);

float numBitsRecv = 0;			// Number of bytes received
SemaphoreHandle_t semaphoreNumBitsRecv;

bool canSendAck = false;
bool canSendData = false;

#define RECEIVER_NUM 1

typedef struct{
	uint8_t macAddr[6];
}mac_t;

mac_t receiverMacArr[] = {
	{{0x54, 0x32, 0x04, 0x07, 0x41, 0xF4}}
};

esp_now_peer_info_t peersArr[RECEIVER_NUM];

uint8_t ack[] = {0xFA};

uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};

void app_main(void) {
	CommsInit();

	PrintMacAddress();

	PeersInit();
	/*semaphoreNumBitsRecv = xSemaphoreCreateMutex();

	xTaskCreatePinnedToCore(MeasureSpeed, "Speed measurement task", 4096, NULL, SPEED_MEASUREMENT_PRIORITY, NULL, tskNO_AFFINITY);
	*/
	while(1) {
		/*if(canSendAck) {
			ESP_ERROR_CHECK(esp_now_send(peersArr[0].peer_addr, ack, sizeof(ack)));
		}

		if(canSendData) {
			ESP_ERROR_CHECK(esp_now_send(peersArr[0].peer_addr, data, sizeof(data)));
		}*/

		vTaskDelay(pdMS_TO_TICKS(10));
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

	ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    ESP_ERROR_CHECK(esp_now_init());

    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
	ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
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

void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {

	// CMD
	if(*data == 0xFF) {
		ESP_LOGI(TAG, "CMD Received");
		canSendAck = true;

		ESP_ERROR_CHECK(esp_now_send(peersArr[0].peer_addr, ack, sizeof(ack)));
	}

	/*xSemaphoreTake(semaphoreNumBitsRecv, pdMS_TO_TICKS(10));
	numBitsRecv += len * 8;
	xSemaphoreGive(semaphoreNumBitsRecv);*/
}

static void espnow_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {
	if(canSendAck) {
		ESP_LOGI(TAG, "ACK sent");
		canSendAck = false;
		canSendData = true;
		ESP_ERROR_CHECK(esp_now_send(peersArr[0].peer_addr, data, sizeof(data)));
	}

	if(canSendData) {
		ESP_LOGI(TAG, "Data sent");
		canSendData = false;
	}
}

#endif