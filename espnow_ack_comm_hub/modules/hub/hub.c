/*******************************BEGIN: PURPOSE OF MODULE*******************************/

/*******************************END: PURPOSE OF MODULE*******************************/



/*******************************BEGIN: MODULE HEADER FILE INCLUDE*******************************/
#include "hub.h"
#include "esp_err.h"
#include "esp_log_buffer.h"
#include "esp_now.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include <string.h>
#include "esp_err.h"			// Required for ESP_ERROR_CHECK()
#include "esp_event.h"			// Required for event driver programming (esp_event_... functions)
#include "esp_wifi.h"			// Required for the Wi-Fi
#include "nvs_flash.h"			// Required for nvs_flash_init()
#include "esp_netif.h"			// Required for esp_netif_init()
#include "esp_now.h"			// Required for ESP-NOW
#include"esp_log.h"
#include <stdint.h>
/*******************************END: MODULE HEADER FILE INCLUDE*******************************/

/*******************************BEGIN: STRUCTS, ENUMS, UNIONS, DEFINES*******************************/
typedef struct {
	uint8_t mac_addr[6];
}PEER_t;

#define TAG "HUB"
#define HUB_COMMUNICATION_TASK_PRIO 5
/*******************************END: GSTRUCTS, ENUMS, UNIONS, DEFINES*******************************/

/*******************************BEGIN: GLOBAL VARIABLES PRIVATE TO MODULE*******************************/
uint8_t cmd[] = {0xFF, 0xFF, 0xFF, 0xFF};

PEER_t peers[] = {
	{.mac_addr = {0xF0, 0xF5, 0xBD, 0x01, 0xB3, 0x48}}			// Add peer MAC addresses here
};

bool ack_recv = false;
bool can_recv_data = false;
bool cmd_sent = false;
bool data_recv = false;
SemaphoreHandle_t semaph_ack_recv;
SemaphoreHandle_t semaph_can_recv_data;
SemaphoreHandle_t semaph_cmd_sent;
SemaphoreHandle_t semaph_data_recv;
#define SEMAPH_ACK_RECV_MS			pdMS_TO_TICKS(5)
#define SEMAPH_CAN_RECV_DATA_MS 	pdMS_TO_TICKS(5)
#define SEMAPH_CMD_SENT_MS			pdMS_TO_TICKS(5) 
#define SEMAPH_DATA_RECV_MS 		pdMS_TO_TICKS(5) 
/*******************************END: GLOBAL VARIABLES PRIVATE TO MODULE*******************************/

/*******************************BEGIN: HELPER FUNCTION PROTOTYPES PRIVATE TO MODULE*******************************/
static void hub_espnow_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status);
static void hub_espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len);
static void hub_connect_peer(PEER_t g_peer);
static void hub_init_tasks(void);
static void hub_communication_task(void *arg);
/*******************************END: HELPER FUNCTION PROTOTYPES PRIVATE TO MODULE*******************************/



/*******************************BEGIN: APIs EXPOSED BY THIS MODULE*******************************/

/*******************************API INFORMATION*******************************
 * @fn			- init_comms()
 * 
 * @brief		- Initializes the communications (Wi-Fi, ESP-NOW)
 * 
 * @param[in]	- none
 * @param[in]	- none
 * @param[in]	- none
 * 
 * @return		- none
 * 
 * @note		- none
 *****************************************************************************/
 void hub_init(void) {
	ESP_ERROR_CHECK(nvs_flash_init());										
												
	ESP_ERROR_CHECK(esp_netif_init());			
	
	ESP_ERROR_CHECK(esp_event_loop_create_default());	
																									
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();									
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));			
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));	
	ESP_ERROR_CHECK(esp_wifi_start());
	ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

	ESP_ERROR_CHECK(esp_now_init());

	ESP_ERROR_CHECK(esp_now_register_send_cb(hub_espnow_send_cb));
	ESP_ERROR_CHECK(esp_now_register_recv_cb(hub_espnow_recv_cb));

	semaph_ack_recv = xSemaphoreCreateMutex();
	semaph_can_recv_data = xSemaphoreCreateMutex();
	semaph_cmd_sent = xSemaphoreCreateMutex();
	semaph_data_recv = xSemaphoreCreateMutex();

	hub_init_tasks();
}
 
/*******************************END: APIs EXPOSED BY THIS MODULE*******************************/


/*******************************API INFORMATION*******************************
 * @fn			- hub_run()
 * 
 * @brief		- 
 * 
 * @param[in]	- none
 * @param[in]	- none
 * @param[in]	- none
 * 
 * @return		- none
 * 
 * @note		- none
 *****************************************************************************/
void hub_run(void) {
	//uint8_t cmd[] = {0xFF, 0xFF, 0xFF, 0xFF};
}
 
 
/*******************************BEGIN: HELPER FUNCTION DEFINITIONS*******************************/

/*******************************FUNCTION INFORMATION*******************************
 * @fn			- espnow_send_cb()
 * 
 * @brief		- 
 * 
 * @param[in]	- Information about the sender and the receiver, and message
 * @param[in]	- Data sent successfully or not
 * @param[in]	- none
 * 
 * @return		- none
 * 
 * @note		- Gets called automatically when the message is sent
 *****************************************************************************/
static void hub_espnow_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {
	xSemaphoreTake(semaph_cmd_sent, SEMAPH_CMD_SENT_MS);
	if(!cmd_sent) {
		cmd_sent = true;
	}
	xSemaphoreGive(semaph_cmd_sent);
	ESP_LOGI(TAG, "CMD sent");
}

/*******************************FUNCTION INFORMATION*******************************
 * @fn			- hub_espnow_recv_cb()
 * 
 * @brief		- 
 * 
 * @param[in]	- Information the message packet
 * @param[in]	- The received data
 * @param[in]	- The length of the received data
 * 
 * @return		- none
 * 
 * @note		- Gets called automatically when a message arrives
 *****************************************************************************/
static void hub_espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
	bool is_ack = false;

	// Checking if message is ACK
	for(uint16_t i = 0; i < len; i++) {
		if(data[i] != 0xEE) {
			break;
		}
		is_ack = true;
	}

	// Message is data
	if(!is_ack ) {
		xSemaphoreTake(semaph_can_recv_data, SEMAPH_CAN_RECV_DATA_MS);
		if(can_recv_data) {
			ESP_LOG_BUFFER_CHAR(TAG, data, len);

			xSemaphoreTake(semaph_data_recv, SEMAPH_DATA_RECV_MS);
			data_recv = true;
			xSemaphoreGive(semaph_data_recv);
		}
		xSemaphoreGive(semaph_can_recv_data);
	}
	else {
		ESP_LOGI(TAG, "ACK received");
		xSemaphoreTake(semaph_ack_recv, SEMAPH_ACK_RECV_MS);
		ack_recv = true;
		xSemaphoreGive(semaph_ack_recv);
	}
}

/*******************************FUNCTION INFORMATION*******************************
 * @fn			- hub_connect_peers()
 * 
 * @brief		- Initializes peers, you can add peers here with their MAC addresses
 * 
 * @param[in]	- none
 * @param[in]	- none
 * @param[in]	- none
 * 
 * @return		- none
 * 
 * @note		- none
 *****************************************************************************/
static void hub_connect_peer(PEER_t g_peer) {
	esp_now_peer_info_t peer = {		
		.channel = 0,					
		.ifidx = ESP_IF_WIFI_STA,		
		.encrypt = false
	};
	memcpy(peer.peer_addr, g_peer.mac_addr, 6);
	ESP_ERROR_CHECK(esp_now_add_peer(&peer));
}

/*******************************FUNCTION INFORMATION*******************************
 * @fn			- hub_init_tasks()
 * 
 * @brief		- 
 * 
 * @param[in]	- none
 * @param[in]	- none
 * @param[in]	- none
 * 
 * @return		- none
 * 
 * @note		- none
 *****************************************************************************/
static void hub_init_tasks(void) {
	// Create a task for hub_send_cmd that repeats .
	xTaskCreatePinnedToCore(hub_communication_task, "Hub task", 4096, (void*)&peers[0], HUB_COMMUNICATION_TASK_PRIO, NULL, tskNO_AFFINITY);
}

static void hub_communication_task(void *arg) {
	PEER_t *peer = (PEER_t*)arg;

	hub_connect_peer(*peer);

	xSemaphoreTake(semaph_cmd_sent, SEMAPH_CMD_SENT_MS);
	cmd_sent = false;
	xSemaphoreGive(semaph_cmd_sent);

	xSemaphoreTake(semaph_ack_recv, SEMAPH_ACK_RECV_MS);
	ack_recv = false;
	xSemaphoreGive(semaph_ack_recv);

	xSemaphoreTake(semaph_can_recv_data, SEMAPH_CAN_RECV_DATA_MS);
	can_recv_data = false;
	xSemaphoreGive(semaph_can_recv_data);

	xSemaphoreTake(semaph_data_recv, SEMAPH_DATA_RECV_MS);
	data_recv = false;
	xSemaphoreGive(semaph_data_recv);

	while(1) {
		// Send cmd if not yet sent
		xSemaphoreTake(semaph_cmd_sent, SEMAPH_CMD_SENT_MS);
		if(!cmd_sent) {
			ESP_ERROR_CHECK(esp_now_send(peer->mac_addr, cmd, sizeof(cmd)));
		}
		xSemaphoreGive(semaph_cmd_sent);

		// Allow data to be received if ack returned
		xSemaphoreTake(semaph_ack_recv, SEMAPH_ACK_RECV_MS);
		if(ack_recv) {
			ack_recv = false;
			xSemaphoreTake(semaph_can_recv_data, SEMAPH_CAN_RECV_DATA_MS);
			can_recv_data = true;
			xSemaphoreGive(semaph_can_recv_data);
		}
		xSemaphoreGive(semaph_ack_recv);

		// If data received, delete task
		xSemaphoreTake(semaph_data_recv, SEMAPH_DATA_RECV_MS);
		if(data_recv) {
			vTaskDelete(NULL);
		}
		xSemaphoreGive(semaph_data_recv);

		// Delay for yield
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}
 /*******************************END: HELPER FUNCTION DEFINITIONS*******************************/
