/*******************************BEGIN: PURPOSE OF MODULE*******************************
* This module implements an ESP32 HUB that serves as the central communication node 
* in an ESP-NOW network. 
*******************************END: PURPOSE OF MODULE*******************************/



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
#define DEBUG_LOG 0

typedef struct {
	uint8_t mac_addr[6];
}PEER_t;

typedef struct {
	int len;
	uint8_t data[ESP_NOW_MAX_DATA_LEN_V2];
}RECEIVE_DATA_t;

#define TAG "HUB"
#define HUB_COMMUNICATION_TASK_PRIO 5
#define HUB_SPEED_MEASUREMENT_TASK_PRIO 5
/*******************************END: GSTRUCTS, ENUMS, UNIONS, DEFINES*******************************/

/*******************************BEGIN: GLOBAL VARIABLES PRIVATE TO MODULE*******************************/
uint8_t cmd[] = {0xFF, 0xFF, 0xFF, 0xFF};
uint8_t ack[] = {0xEE, 0xEE, 0xEE, 0xEE};

PEER_t peers[] = {
	{.mac_addr = {0xF0, 0xF5, 0xBD, 0x01, 0xB3, 0x48}}			// Add peer MAC addresses here
};

/*PEER_t peers[] = {
	{.mac_addr = {0xEC, 0xE3, 0x34, 0x47, 0x66, 0x3C}}		
};*/

double num_bits_recv;

QueueHandle_t send_cb_msg_queue;
QueueHandle_t recv_cb_msg_queue;
SemaphoreHandle_t semaph_num_bits_recv;

double num_cycles, total_time;
SemaphoreHandle_t semaph_num_cycles;
SemaphoreHandle_t semaph_total_time;
/*******************************END: GLOBAL VARIABLES PRIVATE TO MODULE*******************************/

/*******************************BEGIN: HELPER FUNCTION PROTOTYPES PRIVATE TO MODULE*******************************/
static void hub_espnow_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status);
static void hub_espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len);
static void hub_connect_peer(PEER_t g_peer);
static void hub_init_tasks(void);
static void hub_communication_task(void *arg);
void hub_speed_measurement_task(void *arg);
static void hub_print_mac_addr(void);
/*******************************END: HELPER FUNCTION PROTOTYPES PRIVATE TO MODULE*******************************/



/*******************************BEGIN: APIs EXPOSED BY THIS MODULE*******************************/

/*******************************API INFORMATION*******************************
 * @fn			- hub_init()
 * 
 * @brief		- Initializes the communications (Wi-Fi, ESP-NOW), creates 
 *				  semaphores and starts the necessary tasks.
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

	send_cb_msg_queue = xQueueCreate(1, sizeof(esp_now_send_status_t));
	recv_cb_msg_queue = xQueueCreate(1, sizeof(RECEIVE_DATA_t));

	semaph_num_bits_recv = xSemaphoreCreateMutex();
	semaph_num_cycles = xSemaphoreCreateMutex();
	semaph_total_time = xSemaphoreCreateMutex();

	hub_print_mac_addr();
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
 * @note		- Yet to be implemented if needed.
 *****************************************************************************/
void hub_run(void) {

}
 
 
/*******************************BEGIN: HELPER FUNCTION DEFINITIONS*******************************/

/*******************************FUNCTION INFORMATION*******************************
 * @fn			- espnow_send_cb()
 * 
 * @brief		- Passes the result of a send (success or fail) to a communication
 *				  channel (queue).
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
	xQueueSend(send_cb_msg_queue, &status, pdMS_TO_TICKS(5));
}

/*******************************FUNCTION INFORMATION*******************************
 * @fn			- hub_espnow_recv_cb()
 * 
 * @brief		- Passes the data and its length to a queue.
 * 
 * @param[in]	- Information about the message
 * @param[in]	- The received data
 * @param[in]	- The length of the received data
 * 
 * @return		- none
 * 
 * @note		- Gets called automatically when a message arrives
 *****************************************************************************/
static void hub_espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
	RECEIVE_DATA_t recv_data;

	xSemaphoreTake(semaph_num_bits_recv, pdMS_TO_TICKS(10));
	num_bits_recv += len * 8;
	xSemaphoreGive(semaph_num_bits_recv);
	
	if(len > ESP_NOW_MAX_DATA_LEN_V2) {
		len = ESP_NOW_MAX_DATA_LEN_V2;
	}
	
	recv_data.len = len;

	memcpy(recv_data.data, data, len);

	xQueueSend(recv_cb_msg_queue, &recv_data, pdMS_TO_TICKS(5));
}

/*******************************FUNCTION INFORMATION*******************************
 * @fn			- hub_connect_peers()
 * 
 * @brief		- Initializes peers, adds them to the peer list.
 * 
 * @param[in]	- The peer object
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
 * @brief		- Initializes and starts the tasks.
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
	xTaskCreatePinnedToCore(hub_communication_task, "Hub task", 4096, (void*)&peers[0], HUB_COMMUNICATION_TASK_PRIO, NULL, tskNO_AFFINITY);
	xTaskCreatePinnedToCore(hub_speed_measurement_task, "Speed measurement task", 4096, NULL, HUB_SPEED_MEASUREMENT_TASK_PRIO, NULL, tskNO_AFFINITY);
}

/*******************************FUNCTION INFORMATION*******************************
 * @fn			- hub_communication_task()
 * 
 * @brief		- The task that is repsonsible for the communication with the stations
 * 
 * @param[in]	- A pointer to the peer (the other end of the communication).
 * @param[in]	- none
 * @param[in]	- none
 * 
 * @return		- none
 * 
 * @note		- none
 *****************************************************************************/
static void hub_communication_task(void *arg) {
	PEER_t *peer = (PEER_t*)arg;

	hub_connect_peer(*peer);

	esp_now_send_status_t msg_from_send_cb;

	RECEIVE_DATA_t data_from_recv_cb;

	bool cmd_sent;
	bool ack_recv;

	int64_t start_us, end_us;

	while(1) {
		cmd_sent = false;
		ack_recv = false;

		start_us = esp_timer_get_time();
		// Send cmd data, if not successful retry
		ESP_ERROR_CHECK(esp_now_send(peer->mac_addr, cmd, sizeof(cmd)));
		xQueueReceive(send_cb_msg_queue, &msg_from_send_cb, pdMS_TO_TICKS(100));
		if(msg_from_send_cb == ESP_NOW_SEND_SUCCESS) {
#if DEBUG_LOG
			//ESP_LOGI(TAG, "CMD sent successfully.");
#endif
			cmd_sent = true;
		}
#if DEBUG_LOG
		else {
			ESP_LOGI(TAG, "Sending of CMD failed.");
		}
#endif

		// Receive ack data, if not received, then send command again
		if(cmd_sent) {
			ack_recv = false;
			if (xQueueReceive(recv_cb_msg_queue, &data_from_recv_cb, pdMS_TO_TICKS(100)) == pdTRUE) {
				if (data_from_recv_cb.len == sizeof(ack)) {
					ack_recv = true;
					for (uint16_t i = 0; i < sizeof(ack); i++) {
						if (data_from_recv_cb.data[i] != ack[i]) {
							ack_recv = false;
							break;
						}
					}
				}
			}
		}

#if DEBUG_LOG
		// temporary
		if(ack_recv) {
			ESP_LOGI(TAG, "ACK received successfully.");
		}
#endif

		// If ACK received, can receive actual data
		if(ack_recv && xQueueReceive(recv_cb_msg_queue, &data_from_recv_cb, pdMS_TO_TICKS(100)) == pdTRUE) {
#if DEBUG_LOG
			ESP_LOG_BUFFER_CHAR(TAG, data_from_recv_cb.data, data_from_recv_cb.len);
			ESP_LOGI(TAG, "Data received successfully.");
#endif
			//vTaskDelete(NULL);
		}

		end_us = esp_timer_get_time();

		xSemaphoreTake(semaph_total_time, pdMS_TO_TICKS(10));
		total_time += end_us - start_us;
		xSemaphoreGive(semaph_total_time);

		xSemaphoreTake(semaph_num_cycles, pdMS_TO_TICKS(10));
		num_cycles++;
		xSemaphoreGive(semaph_num_cycles);
		
		// Delay for yield
		vTaskDelay(pdMS_TO_TICKS(1));
	}
}

/*******************************FUNCTION INFORMATION*******************************
 * @fn			- hub_speed_measurement_task()
 * 
 * @brief		- The task that is repsonsible for logging the latency and 
 *              - throughput information.
 * 
 * @param[in]	- NULL
 * @param[in]	- none
 * @param[in]	- none
 * 
 * @return		- none
 * 
 * @note		- none
 *****************************************************************************/
void hub_speed_measurement_task(void *arg) {
	double avg_delay;

	while(1) {
		xSemaphoreTake(semaph_num_bits_recv, pdMS_TO_TICKS(10));
		ESP_LOGI(TAG, "Speed: %.2f kBits/s", num_bits_recv / 1000);
		num_bits_recv = 0;
		xSemaphoreGive(semaph_num_bits_recv);

		xSemaphoreTake(semaph_num_cycles, pdMS_TO_TICKS(10));
		xSemaphoreTake(semaph_total_time, pdMS_TO_TICKS(10));
		ESP_LOGI(TAG, "Number of communication cycles in a second: %d", (int)num_cycles);
		avg_delay = total_time / num_cycles;
		num_cycles = 0;
		total_time = 0;
		xSemaphoreGive(semaph_num_cycles);
		xSemaphoreGive(semaph_total_time);

		ESP_LOGI(TAG, "Average latency of communication cycles in a second: %.2f us", avg_delay);


		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

/*******************************FUNCTION INFORMATION*******************************
 * @fn			- hub_print_mac_addr()
 * 
 * @brief		- Prints the MAC address of the device on boot
 * 
 * @param[in]	- none
 * @param[in]	- none
 * @param[in]	- none
 * 
 * @return		- none
 * 
 * @note		- none
 *****************************************************************************/
static void hub_print_mac_addr(void) {
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_wifi_get_mac(ESP_IF_WIFI_STA, mac));
    ESP_LOGI("MAC_ADDRESS", "Receiver MAC: %02X:%02X:%02X:%02X:%02X:%02X", 
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
 /*******************************END: HELPER FUNCTION DEFINITIONS*******************************/
