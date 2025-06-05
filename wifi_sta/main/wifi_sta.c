/*******************************BEGIN: PURPOSE OF MODULE*******************************/

/*This is a Wi-Fi station mode TCP client implementation for the ESP32, 
which connects to a softAP with SSID "ESP_AP_TESZT" and sends a 1024-byte data packet ('A's) 
continuously to a server listening on 192.168.4.1:12345.*/

/*******************************END: PURPOSE OF MODULE*******************************/



/*******************************BEGIN: MODULE HEADER FILE INCLUDE*******************************/
#include "esp_err.h"
#include "esp_event.h"

#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_netif_ip_addr.h"

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>

/*******************************END: MODULE HEADER FILE INCLUDE*******************************/



/*******************************BEGIN: GLOBAL VARIABLES PRIVATE TO MODULE*******************************/

#define TAG "ESP_STA"    //used to tag identifier in ESP logging (to distinguish messages)
#define PORT 12345       //defines the port of the TCP server the ESP32 will connect to
#define SERVER_IP "192.168.4.1"    //defines the ip address of the AP

static bool is_connected = false;   //a flag to indicate whether the ESP32 has succesfully connected to Wi-Fi

/*******************************END: GLOBAL VARIABLES PRIVATE TO MODULE*******************************/



/*******************************BEGIN: HELPER FUNCTION DEFINITIONS*******************************/

/*******************************FUNCTION INFORMATION*******************************
 * @fn		- 
 * 
 * @brief	- callback function for handling Wi-Fi an IP events
 * 
 * @param[in]	- event_base
 * @param[in]	- event_id
 * @param[in]	- event_data
 * 
 * @return	- void
 * 
 * @note	- 
 *****************************************************************************/
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {       //if station mode starts, initiate connection to AP
		esp_wifi_connect();
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {    //if disconnected, automatically reconnect to AP
		ESP_LOGI(TAG, "Disconnected. Reconnecting...");
		esp_wifi_connect();
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {   //when IP address is obtained from AP, print it and set connection flag
		ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
		ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));     //log the IP address obtained from AP
		is_connected = true;
	}
}
 /*******************************END: HELPER FUNCTION DEFINITIONS*******************************/

/*******************************BEGIN: APIs EXPOSED BY THIS MODULE*******************************/

/*******************************API INFORMATION*******************************
 * @fn		- app_main
 * 
 * @brief	- main function, creates Wi-Fi station and sends out data to the AP
 * 
 * @param[in]	- void
 * 
 * @return	- void
 * 
 * @note	- 
 *****************************************************************************/
void app_main(void)
{
	ESP_ERROR_CHECK(nvs_flash_init());    // ESP_ERROR_CHECK ==> returns ESP_OK if no error happened, otherwise prints error message
						// nvs_flash_init() ==> Initializes the default NVS (Non Volatile Storage) partition, 
						//required for many components (Wi-Fi, Bluetooth, ESP-NOW) to store and retrieve settings in non-volatile storage
	ESP_ERROR_CHECK(esp_netif_init());     //esp_netif_init() ==> It's used to initialize the networking stack (NETIF). Required for network interfaces (STA, AP)
	ESP_ERROR_CHECK(esp_event_loop_create_default());    // esp_event_loop_create_default() ==> Handles system events (eg. Wi-Fi connection events, ESP-NOW events).

	esp_netif_create_default_wifi_sta();   //creates a deafult network interface instance for the Wi-Fi station mode, internally creates esp_netif_t object, configures it with
	                                       // default TCP/IP settings, binds it to the station interface WIFI_IF_STA, needded for DHCP and IP event handling to work correctly

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();  // wifi_init_config_t ==> A structure that contains the initialization values for the Wi-Fi driver
							      //WIFI_INIT_CONFIG_DEFAULT() ==> Assign the default values to the struct (only needs to be other when doing advanced tuning)
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));    // esp_wifi_init() ==> Initializes the Wi-Fi driver (with the cfg structure passed as parameter)

	//defines the parameters used to connect to the AP
	wifi_config_t wifi_config = {
		.sta = {
			.channel = 3,    //sets the Wi-Fi channel
			.ssid = "ESP_AP_TESZT", 
			.password = "", 
			.threshold.authmode = WIFI_AUTH_OPEN,   //accept any open network
			.pmf_cfg = {                           //disables Protected Management Frames 
				.capable = false,
				.required = false
			},
		},
	};

	
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL)); //registers event handler function for Wi-Fi events
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));  //registers event handler function for IP event (when DHCP gives the station an IP)

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));   //sets Wi-Fi mode to station
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));   //applies wifi_config to the WIFI_IF_STA interface (station)
	ESP_ERROR_CHECK(esp_wifi_start());   //starts the Wi-Fi driver, starts the background Wi-Fi task, Triggers the WIFI_EVENT_STA_START event

	ESP_LOGI(TAG, "Connecting to AP...");

	// busy wait loop until youre device gets an IP from the access point
	while (!is_connected) {
		vTaskDelay(100 / portTICK_PERIOD_MS);   //sleeps for 100 ms in each loop iteration to avoid locking the CPU.
	}

	int sock = socket(AF_INET, SOCK_STREAM, 0);   //creates a new socket, AF_INET ==> use IPv4, SOCK_STREAM,
	if (sock < 0) {
		ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);   //log if the socket creation failed
		return;
	}

	struct sockaddr_in server_addr = {     //prepares the server access architecture
		.sin_family = AF_INET,
		.sin_port = htons(PORT),
	};
	inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);    //converts ip string to ip address

	//tries to connect the server using the sock socket and the server_addr structure
	while (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
		ESP_LOGE(TAG, "TCP connect failed: errno %d", errno);
		vTaskDelay(1000 / portTICK_PERIOD_MS);           //waits 1 second until the connection succeeds
	}

	ESP_LOGI(TAG, "Connected to server, start sending data");    //ready to send data

	uint8_t data[1024];         //buffer to store data
	memset(data, 'A', sizeof(data));   //fills the buffer with As	

	//keep sending data until failure
	while (1) {
		int err = send(sock, data, sizeof(data), 0);
		if (err < 0) {
			ESP_LOGE(TAG, "Send failed: errno %d", errno);    //logs error
			break;
		}
	}
	close(sock); //closes the socket when done
}
/*******************************END: APIs EXPOSED BY THIS MODULE*******************************/

