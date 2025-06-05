/*******************************BEGIN: PURPOSE OF MODULE*******************************/

// This module implements the AP (Access Point) mode of ESP32 Wi-Fi communication.
// The module creates an access point and accepts a clients (stations).
// The module measures the speed of communication with the station.

/*******************************END: PURPOSE OF MODULE*******************************/



/*******************************BEGIN: MODULE HEADER FILE INCLUDE*******************************/

#include "esp_err.h"			// Required for ESP_ERROR_CHECK()
#include "esp_event.h"			// Required for event driver programming (esp_event_... functions)
#include "esp_wifi.h"			// Required for the Wi-Fi
#include "nvs_flash.h"			// Required for nvs_flash_init()
#include "esp_netif.h"			// Required for esp_netif_init()
#include "string.h"				// Required for memcpy
#include "esp_log.h"           	// <-- For ESP_LOGI, ESP_LOGE, etc.
#include "esp_mac.h"          	 // <-- For MACSTR and MAC2STR macros
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "esp_timer.h"

/*******************************END: MODULE HEADER FILE INCLUDE*******************************/

/*******************************BEGIN: DEFINES PRIVATE TO MODULE*******************************/

#define TAG	"ESP_AP" 	
#define PORT 12345

/*******************************END: DEFINES PRIVATE TO MODULE*******************************/

/*******************************BEGIN: GLOBAL VARIABLES PRIVATE TO MODULE*******************************/

/*******************************END: GLOBAL VARIABLES PRIVATE TO MODULE*******************************/

/*******************************BEGIN: HELPER FUNCTION PROTOTYPES PRIVATE TO MODULE*******************************/

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void accept_clients(void);

/*******************************END: HELPER FUNCTION PROTOTYPES PRIVATE TO MODULE*******************************/



/*******************************BEGIN: APIs EXPOSED BY THIS MODULE*******************************/

/*******************************API INFORMATION*******************************
 * @fn			- app_main()
 * 
 * @brief		- Initializes the neccessary modules for setting up an access point, then calls the function responsible for accepting clients
 * 
 * @param[in]	- none
 * @param[in]	- none
 * @param[in]	- none
 * 
 * @return		- none
 * 
 * @note		- none
 *****************************************************************************/
 void app_main(void)
{
	// ESP_ERROR_CHECK ==> returns ESP_OK if no error happened, otherwise prints error message
	// nvs_flash_init() ==> Initializes the default NVS (Non Volatile Storage) partition, 
	// NVS is required for many components (Wi-Fi, Bluetooth, ESP-NOW) to store and retrieve settings in non-volatile storage
	ESP_ERROR_CHECK(nvs_flash_init());

	// 	esp_netif_init() ==> It's used to initialize the networking stack (NETIF). Required for network interfaces (STA, AP)		
	ESP_ERROR_CHECK(esp_netif_init());

	// esp_event_loop_create_default() ==> Handles system events (eg. Wi-Fi connection events, ESP-NOW events).
	// Internally used by Wi-Fi and ESP-NOW components (that's why required)
	// Can also be used to handle events like Wi-Fi disconnected, package sent etc.
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	// esp_netif_create_default_wifi_ap() ==> Creates the default network interface for AP mode. Sets up DHCP and IP stack for AP
	// Connects the Wi-Fi driver to the TCP/IP layer, required for AP mode to handle IP-based networking
	esp_netif_create_default_wifi_ap();

	// wifi_init_config_t ==> A structure that contains the initialization values for the Wi-Fi driver
	// WIFI_INIT_CONFIG_DEFAULT() ==> Assigns the default values to the struct (only needs to be other when doing advanced tuning)
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	// Configuration data for device's AP or STA or NAN.
	wifi_config_t wifi_config = {
		.ap = {
			.ssid = "ESP_AP_TESZT",
			.ssid_len = strlen("ESP_AP_TESZT"),
			.channel = 3,
			.max_connection = 4,
			.authmode = WIFI_AUTH_OPEN
		}
	};

	// esp_event_handler_register() ==> Register the wifi_event_handler() callback for all Wi-Fi events (WIFI_EVENT base with any event ID)
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

	// esp_wifi_set_mode() ==> Sets the Wi-Fi to AP (Access Point) mode
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

	// esp_wifi_set_config() ==> Passes the configuration structure to the Wi-Fi AP driver 
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));

	// esp_wifi_start() ==> Starts the WiFi driver
	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_LOGI(TAG, "Access Point started");

	// accept_clients() ==> Starts accepting clients
	accept_clients();
}
 
/*******************************END: APIs EXPOSED BY THIS MODULE*******************************/
 
 
 
/*******************************BEGIN: HELPER FUNCTION DEFINITIONS*******************************/

/*******************************FUNCTION INFORMATION*******************************
 * @fn			- wifi_event_handler()
 * 
 * @brief		- Handles any Wi-Fi event (current implementation os only for connection and disconnection events). Logs info about station.
 * 
 * @param[in]	- User defined in the esp_event_handler_register() call, in this case NULL
 * @param[in]	- Responsibility of the abstraction layers
 * @param[in]	- Responsibility of the abstraction layers
 * @param[in]	- Responsibility of the abstraction layers
 * 
 * @return		- none
 * 
 * @note		- none
 *****************************************************************************/
 static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
	if (event_base == WIFI_EVENT) {
		if (event_id == WIFI_EVENT_AP_STACONNECTED) {
			wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
			ESP_LOGI(TAG, "Station "MACSTR" joined, AID=%d", MAC2STR(event->mac), event->aid);
		} else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
			wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
			ESP_LOGI(TAG, "Station "MACSTR" left, AID=%d", MAC2STR(event->mac), event->aid);
		}
	}
}

/*******************************FUNCTION INFORMATION*******************************
 * @fn			- accept_clients()
 * 
 * @brief		- Handles any Wi-Fi event (current implementation os only for connection and disconnection events). Logs info about station.
 * 
 * @param[in]	- User defined in the esp_event_handler_register() call, in this case NULL
 * @param[in]	- Responsibility of the abstraction layers
 * @param[in]	- Responsibility of the abstraction layers
 * @param[in]	- Responsibility of the abstraction layers
 * 
 * @return		- none
 * 
 * @note		- none
 *****************************************************************************/
static void accept_clients(void) {
	// Creates a socket (IPv4, TCP)
	int listen_sock = socket(AF_INET, SOCK_STREAM, 0);

	// A structure that is used to specify the address and port for a socket
	struct sockaddr_in addr = {
		.sin_family = AF_INET,					// IPv4
		.sin_addr.s_addr = INADDR_ANY,			// Accept any connections
		.sin_port = htons(PORT),		// Port converted to big endian (network byte order)
	};

	// Sets up where the socket will listen to incoming connections ()
	bind(listen_sock, (struct sockaddr*)&addr, sizeof(addr));

	// Starts listening, only a maximum of one queue is accepted
	listen(listen_sock, 1);

	ESP_LOGI(TAG, "Waiting for incoming connection...");

	// Acceps the queued client, returns a descriptor that is used to handle communication 
	int sock = accept(listen_sock, NULL, NULL);

	ESP_LOGI(TAG, "Client connected");

	uint8_t buffer[1024];
	int total_bytes = 0;
	int64_t start_time = esp_timer_get_time();

	while (1) {
		// Reads incoming data from client socket sock into buffer.
		// Returns number of bytes received (len), or 0 if connection closed, or negative on error.
		int len = recv(sock, buffer, sizeof(buffer), 0);

		if (len <= 0) break;
		total_bytes += len;

		int64_t now = esp_timer_get_time();
		float elapsed_sec = (now - start_time) / 1000000.0;

		if (elapsed_sec >= 1.0) {
			// Calculates bits per second
			float speed_bps = (total_bytes * 8) / elapsed_sec;
			ESP_LOGI(TAG, "Received %d bytes in %.2f sec (%.2f kbps)",
				total_bytes, elapsed_sec, speed_bps / 1000);
			total_bytes = 0;
			start_time = now;
		}
	}

	close(sock);
	close(listen_sock);
}
 
 /*******************************END: HELPER FUNCTION DEFINITIONS*******************************/
