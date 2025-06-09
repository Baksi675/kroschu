#if 0

#define ROOT 1

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_mesh.h"
#include "esp_netif.h"

static const char *TAG = "mesh_node";
#define MESH_ID {0x7D, 0x4C, 0x12, 0x23, 0x45, 0x56}

void mesh_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    switch (event_id) {
        case MESH_EVENT_STARTED:
            ESP_LOGI(TAG, "MESH_EVENT_STARTED");
            break;
        case MESH_EVENT_PARENT_CONNECTED:
            ESP_LOGI(TAG, "MESH_EVENT_PARENT_CONNECTED");
            break;
        case MESH_EVENT_PARENT_DISCONNECTED:
            ESP_LOGI(TAG, "MESH_EVENT_PARENT_DISCONNECTED");
            break;
        default:
            ESP_LOGI(TAG, "Other event: %d", event_id);
            break;
    }
}

void mesh_send_task(void *arg) {
    esp_err_t err;
    mesh_data_t data;
    uint8_t buf[128] = "Hello from node!";
    data.data = buf;
    data.size = strlen((char *)buf) + 1;
    data.proto = MESH_PROTO_BIN;
    data.tos = MESH_TOS_P2P;

    while (true) {
        if (esp_mesh_is_root()) {
            // Root broadcasts message to all nodes
            err = esp_mesh_send(NULL, &data, 0, NULL, 0);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "Root broadcast message");
            } else {
                ESP_LOGE(TAG, "Root esp_mesh_send failed: %s", esp_err_to_name(err));
            }
        } else {
            // Non-root nodes send message to parent/root
            err = esp_mesh_send(NULL, &data, 0, NULL, 0);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "Node sent message to root");
            } else {
                ESP_LOGE(TAG, "Node esp_mesh_send failed: %s", esp_err_to_name(err));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    /*mesh_cfg_t mesh_cfg = MESH_INIT_CONFIG_DEFAULT();
    uint8_t mesh_id[6] = MESH_ID;
    memcpy(mesh_cfg.mesh_id.addr, mesh_id, 6);
    mesh_cfg.channel = 3; // Auto channel
    mesh_cfg.router.ssid[0] = '\0';  // No router (no internet)
    mesh_cfg.router.password[0] = '\0';
    mesh_cfg.mesh_ap.password[0] = '\0'; // No mesh AP password*/

mesh_cfg_t mesh_cfg = MESH_INIT_CONFIG_DEFAULT();

uint8_t mesh_id[6] = {0x7D, 0x4C, 0x12, 0x23, 0x45, 0x56};
memcpy(mesh_cfg.mesh_id.addr, mesh_id, 6);

mesh_cfg.channel = 6;  // Fixed WiFi channel, no zero!

// Not connecting to external router
mesh_cfg.router.ssid[0] = '\0';
mesh_cfg.router.password[0] = '\0';
mesh_cfg.router.bssid[0] = 0x00; // zero out BSSID as well

// Mesh softAP config
mesh_cfg.mesh_ap.max_connection = 6;
mesh_cfg.mesh_ap.nonmesh_max_connection = 0;
memset(mesh_cfg.mesh_ap.password, 0, sizeof(mesh_cfg.mesh_ap.password));  // empty password for open mesh AP, or set your password


    ESP_ERROR_CHECK(esp_mesh_init());
    ESP_ERROR_CHECK(esp_event_handler_register(MESH_EVENT, ESP_EVENT_ANY_ID, &mesh_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mesh_set_config(&mesh_cfg));

#if ROOT
    ESP_ERROR_CHECK(esp_mesh_set_type(MESH_ROOT));
#else
    ESP_ERROR_CHECK(esp_mesh_set_type(MESH_NODE));
#endif

    ESP_ERROR_CHECK(esp_mesh_start());

    ESP_LOGI(TAG, "Mesh started");

    xTaskCreate(mesh_send_task, "mesh_send_task", 4096, NULL, 5, NULL);
}

#endif

#define ROOT 0
#define RELAY 0
#define SENDER 1

#if ROOT

#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
#include "esp_netif.h"
#include "esp_timer.h"

#define AP_SSID "esp32_root_ap"
#define AP_PASS "12345678"
#define TCP_PORT 1234

static const char *TAG = "ROOT";

void wifi_init_ap(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t ap_config = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = strlen(AP_SSID),
            .password = AP_PASS,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    esp_wifi_start();
}

void tcp_server_task(void *pvParameters) {
    char rx_buffer[128];
    struct sockaddr_in listen_addr;
    listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_port = htons(TCP_PORT);

    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    bind(listen_sock, (struct sockaddr *)&listen_addr, sizeof(listen_addr));
    listen(listen_sock, 1);

    ESP_LOGI(TAG, "TCP server listening on port %d", TCP_PORT);

    while (1) {
        struct sockaddr_in source_addr;
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);

        while (1) {
            int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            if (len <= 0) break;
            rx_buffer[len] = 0;

            // Log timestamp and data
            int64_t now = esp_timer_get_time();
            ESP_LOGI(TAG, "Received (%lld µs): %s", now, rx_buffer);
        }

        close(sock);
        ESP_LOGI(TAG, "Sender disconnected.");
    }
}

void app_main(void) {
    nvs_flash_init();
    wifi_init_ap();
    xTaskCreate(tcp_server_task, "tcp_server", 4096, NULL, 5, NULL);
}

#endif

#if RELAY

#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
#include "esp_netif.h"
#include "esp_timer.h"

#define STA_SSID "esp32_root_ap"
#define STA_PASS "12345678"

#define AP_SSID "esp32_root_ap"
#define AP_PASS "12345678"

#define TCP_PORT 1234

static const char *TAG = "ROOT";			// Relay

void wifi_init_ap(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t ap_config = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = strlen(AP_SSID),
            .password = AP_PASS,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    esp_wifi_start();
}

void tcp_server_task(void *pvParameters) {
    char rx_buffer[128];
    struct sockaddr_in listen_addr;
    listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_port = htons(TCP_PORT);

    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    bind(listen_sock, (struct sockaddr *)&listen_addr, sizeof(listen_addr));
    listen(listen_sock, 1);

    ESP_LOGI(TAG, "TCP server listening on port %d", TCP_PORT);

    while (1) {
        struct sockaddr_in source_addr;
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);

        while (1) {
            int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            if (len <= 0) break;
            rx_buffer[len] = 0;

            // Log timestamp and data
            int64_t now = esp_timer_get_time();
            ESP_LOGI(TAG, "Received (%lld µs): %s", now, rx_buffer);
        }

        close(sock);
        ESP_LOGI(TAG, "Sender disconnected.");
    }
}

void app_main(void) {
    nvs_flash_init();
    wifi_init_ap();
    xTaskCreate(tcp_server_task, "tcp_server", 4096, NULL, 5, NULL);
}


#endif

#if SENDER

// File: main.c
// Role: Sender
#include <string.h>
#include <stdio.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "lwip/sockets.h"
#include "esp_timer.h"

#define STA_SSID "esp32_child_ap"
#define STA_PASS "12345678"
#define TCP_PORT 1234
#define RELAY_IP "192.168.4.1"  // IP of relay AP

static const char *TAG = "SENDER";

void wifi_init_sta(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t sta_config = {
        .sta = {
            .ssid = STA_SSID,
            .password = STA_PASS,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &sta_config);
    esp_wifi_start();
    esp_wifi_connect();
}

void send_task(void *pvParameters) {
    vTaskDelay(pdMS_TO_TICKS(3000)); // wait for connection

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(RELAY_IP);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(TCP_PORT);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));

    for (int i = 0; i < 10; i++) {
        int64_t timestamp = esp_timer_get_time();
        char msg[128];
        snprintf(msg, sizeof(msg), "Message %d @ %lld", i, timestamp);
        send(sock, msg, strlen(msg), 0);
        ESP_LOGI(TAG, "Sent: %s", msg);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    close(sock);
}

void app_main(void) {
    nvs_flash_init();
    wifi_init_sta();
    xTaskCreate(send_task, "send_task", 4096, NULL, 5, NULL);
}


#endif