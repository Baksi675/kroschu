/*******************************BEGIN: PURPOSE OF MODULE*******************************/

/*******************************END: PURPOSE OF MODULE*******************************/



/*******************************BEGIN: MODULE HEADER FILE INCLUDE*******************************/
#include "station.h"
#include "esp_err.h"
#include "esp_log_buffer.h"
#include "esp_now.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "esp_err.h"			// Required for ESP_ERROR_CHECK()
#include "esp_event.h"			// Required for event driver programming (esp_event_... functions)
#include "esp_wifi.h"			// Required for the Wi-Fi
#include "nvs_flash.h"			// Required for nvs_flash_init()
#include "esp_netif.h"			// Required for esp_netif_init()
#include "esp_now.h"			// Required for ESP-NOW
#include"esp_log.h"
#include <string.h>
/*******************************END: MODULE HEADER FILE INCLUDE*******************************/

/*******************************BEGIN: STRUCTS, ENUMS, UNIONS, DEFINES*******************************/
typedef struct {
	uint8_t mac_addr[6];
}PEER_t;

#define TAG "STATION"
#define STA_COMMUNICATION_TASK_PRIO 5
/*******************************END: GSTRUCTS, ENUMS, UNIONS, DEFINES*******************************/

/*******************************BEGIN: GLOBAL VARIABLES PRIVATE TO MODULE*******************************/
uint8_t ack[] = {0xEE, 0xEE, 0xEE, 0xEE};

PEER_t peers[] = {
	{.mac_addr = {0x54, 0x32, 0x04, 0x07, 0x41, 0xF4}}			// Add peer MAC addresses here
};

uint8_t data[ESP_NOW_MAX_DATA_LEN_V2];

bool cmd_recv;
bool ack_sent;
bool data_sent;

SemaphoreHandle_t semaph_cmd_recv;
SemaphoreHandle_t semaph_ack_sent;
SemaphoreHandle_t semaph_data_sent;

#define SEMAPH_CMD_RECV_MS	pdMS_TO_TICKS(5)
#define SEMAPH_ACK_SENT_MS	pdMS_TO_TICKS(5)
#define SEMAPH_DATA_SENT_MS	pdMS_TO_TICKS(5)


/*******************************END: GLOBAL VARIABLES PRIVATE TO MODULE*******************************/

/*******************************BEGIN: HELPER FUNCTION PROTOTYPES PRIVATE TO MODULE*******************************/
static void sta_espnow_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status);
static void sta_espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len);
static void sta_connect_peer(PEER_t g_peer);
static void sta_init_tasks(void);
static void sta_communication_task(void *arg);
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
 void sta_init(void) {
	ESP_ERROR_CHECK(nvs_flash_init());										
												
	ESP_ERROR_CHECK(esp_netif_init());			
	
	ESP_ERROR_CHECK(esp_event_loop_create_default());	
																									
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();									
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));			
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));	
	ESP_ERROR_CHECK(esp_wifi_start());
	ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

	ESP_ERROR_CHECK(esp_now_init());

	ESP_ERROR_CHECK(esp_now_register_send_cb(sta_espnow_send_cb));
	ESP_ERROR_CHECK(esp_now_register_recv_cb(sta_espnow_recv_cb));

	semaph_cmd_recv = xSemaphoreCreateBinary();
	semaph_ack_sent = xSemaphoreCreateBinary();
	semaph_data_sent = xSemaphoreCreateBinary();

	memset(data, 'A', sizeof(data));

	sta_init_tasks();
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
 void sta_run(void) {

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
 static void sta_espnow_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {
	bool is_ack = true;

	// Set ack_sent to true
	for (int i = 0; i < sizeof(ack); i++) {
		if (tx_info->data[i] != ack[i]) {
			is_ack = false;
			break;
		}
	}

	if(is_ack) {
		ESP_LOGI(TAG, "ACK sent");
		xSemaphoreTake(semaph_ack_sent, SEMAPH_ACK_SENT_MS);
		ack_sent = true;
		xSemaphoreGive(semaph_ack_sent);
	}
	/*else {
		xSemaphoreTake(semaph_data_sent, SEMAPH_DATA_SENT_MS);
		data_sent = true;
		xSemaphoreGive(semaph_data_sent);
	}*/

 }

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
static void sta_espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
	bool is_cmd = false;
	
	// Checking if message is CMD
	for(uint16_t i = 0; i < len; i++) {
		if(data[i] != 0xFF) {
			break;
		}
		is_cmd = true;
	}

	// Message is CMD
	if(is_cmd) {
		ESP_LOGI(TAG, "CMD received");
		xSemaphoreTake(semaph_cmd_recv, SEMAPH_CMD_RECV_MS);
		cmd_recv = true;
		xSemaphoreGive(semaph_cmd_recv);
	}
}

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
 static void sta_connect_peer(PEER_t g_peer) {
	esp_now_peer_info_t peer = {		
		.channel = 0,					
		.ifidx = ESP_IF_WIFI_STA,		
		.encrypt = false
	};
	memcpy(peer.peer_addr, g_peer.mac_addr, 6);
	ESP_ERROR_CHECK(esp_now_add_peer(&peer));
 }

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
 static void sta_init_tasks(void) {
	xTaskCreatePinnedToCore(sta_communication_task, "Station task", 4096, (void*)&peers[0], STA_COMMUNICATION_TASK_PRIO, NULL, tskNO_AFFINITY);
 }

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
 static void sta_communication_task(void *arg) {
	PEER_t *peer = (PEER_t*)arg;

	sta_connect_peer(*peer);

	xSemaphoreTake(semaph_cmd_recv, SEMAPH_CMD_RECV_MS);
	cmd_recv = false;
	xSemaphoreGive(semaph_cmd_recv);

	xSemaphoreTake(semaph_ack_sent, SEMAPH_ACK_SENT_MS);
	ack_sent = false;
	xSemaphoreGive(semaph_ack_sent);

	xSemaphoreTake(semaph_data_sent, SEMAPH_DATA_SENT_MS);
	data_sent = false;
	xSemaphoreGive(semaph_data_sent);
	while(1) {
		// Send ACK if CMD received
		xSemaphoreTake(semaph_cmd_recv, SEMAPH_CMD_RECV_MS);
		if(cmd_recv) {
			cmd_recv = false;
			ESP_ERROR_CHECK(esp_now_send(peer->mac_addr, ack, sizeof(ack)));
		}
		xSemaphoreGive(semaph_cmd_recv);

		// Send data after ACK sent
		xSemaphoreTake(semaph_ack_sent, SEMAPH_ACK_SENT_MS);
		if(ack_sent) {
			ack_sent = false;
			ESP_ERROR_CHECK(esp_now_send(peer->mac_addr, data, sizeof(data)));
			data_sent = true;
		}
		xSemaphoreGive(semaph_ack_sent);

		// If data has been send, delete task
		xSemaphoreTake(semaph_data_sent, SEMAPH_DATA_SENT_MS);
		if(data_sent) {
			ESP_LOGI(TAG, "delete");
			vTaskDelete(NULL);
		}
		xSemaphoreGive(semaph_data_sent);

		// Delay to yield CPU
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
 }

 /*******************************END: HELPER FUNCTION DEFINITIONS*******************************/