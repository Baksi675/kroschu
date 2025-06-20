/*******************************BEGIN: PURPOSE OF MODULE*******************************

*******************************END: PURPOSE OF MODULE*******************************/



/*******************************BEGIN: MODULE HEADER FILE INCLUDE*******************************/
#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_fat.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "argtable3/argtable3.h"
#include "hub.h"
#include "stats.h"
/*******************************END: MODULE HEADER FILE INCLUDE*******************************/

/*******************************BEGIN: SETTINGS*******************************/
#if SOC_USB_SERIAL_JTAG_SUPPORTED
#if !CONFIG_ESP_CONSOLE_SECONDARY_NONE
#warning "A secondary serial console is not useful when using the console component. Please disable it in menuconfig."
#endif
#endif
/*******************************END: SETTINGS*******************************/

/*******************************BEGIN: STRUCTS, ENUMS, UNIONS, DEFINES*******************************/
static const char* TAG = "CCONSOLE";
#define PROMPT_STR CONFIG_IDF_TARGET

#define CONFIG_CONSOLE_MAX_COMMAND_LINE_LENGTH 1024

static struct {
    struct arg_str *mac;
    struct arg_lit *all;
    struct arg_lit *loop;
    struct arg_end *end;
} args;
/*******************************END: STRUCTS, ENUMS, UNIONS, DEFINES*******************************/

/*******************************BEGIN: GLOBAL VARIABLES PRIVATE TO MODULE*******************************/
static void *argtable[] = {
    NULL, NULL, NULL, NULL  // Filled in during registration
};
/*******************************END: GLOBAL VARIABLES PRIVATE TO MODULE*******************************/

/*******************************BEGIN: HELPER FUNCTION PROTOTYPES PRIVATE TO MODULE*******************************/
static void initialize_nvs(void);
int send_cmd_to_sta(int argc, char **argv);
static void register_mac_command(void);
static void register_measurement_command(void);
static int start_measurement(int argc, char **argv);
static void register_stats_command(void);
/*******************************END: HELPER FUNCTION PROTOTYPES PRIVATE TO MODULE*******************************/



/*******************************BEGIN: APIs EXPOSED BY THIS MODULE*******************************/

/*******************************API INFORMATION*******************************
 * @fn			- 
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
 void cconsole_init(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();

    repl_config.prompt = PROMPT_STR ">";
    repl_config.max_cmdline_length = CONFIG_CONSOLE_MAX_COMMAND_LINE_LENGTH;

    initialize_nvs();

#if CONFIG_CONSOLE_STORE_HISTORY
    initialize_filesystem();
    repl_config.history_save_path = HISTORY_PATH;
    ESP_LOGI(TAG, "Command history enabled");
#else
    ESP_LOGI(TAG, "Command history disabled");
#endif

    /* Register commands */
	register_mac_command();
    esp_console_register_help_command();
	register_measurement_command();
	register_stats_command();

#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));

#elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
    esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl));

#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));

#else
#error Unsupported console type
#endif

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
 
/*******************************END: APIs EXPOSED BY THIS MODULE*******************************/
 
 
/*******************************BEGIN: HELPER FUNCTION DEFINITIONS*******************************/

static void initialize_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

int send_cmd_to_sta(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, argtable);
    if (nerrors != 0) {
        arg_print_errors(stderr, args.end, argv[0]);
        return 1;
    }

    bool mac_given = args.mac->count > 0;
    bool all_given = args.all->count > 0;
    bool loop = args.loop->count > 0;

    if (mac_given && all_given) {
        printf("Error: --mac and --all options are mutually exclusive.\n");
        return 1;
    }

    if (!mac_given && !all_given) {
        printf("Error: Either --mac or --all must be provided.\n");
        return 1;
    }

    if (mac_given) {
        const char *mac_str = args.mac->sval[0];
        uint8_t mac[6];

        if (sscanf(mac_str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                   &mac[0], &mac[1], &mac[2],
                   &mac[3], &mac[4], &mac[5]) != 6) {
            printf("Invalid MAC address format: %s\n", mac_str);
            return 1;
        }

        PEER_t peer;
        memcpy(peer.mac_addr, mac, sizeof(mac));

        if (loop) {
            hub_spawn_comm_loop_task(peer);
        } else {
            hub_spawn_comm_task(peer);
        }
    } else if (all_given) {
        if (loop) {
            hub_spawn_comm_all_loop_task(true);
        } else {
            hub_spawn_comm_all_loop_task(false);
        }
    }

    return 0;
}



static int start_measurement(int argc, char **argv) {
    static struct {
        struct arg_str *action;
        struct arg_end *end;
    } args;

    args.action = arg_str1(NULL, "action", "<start|stop>", "Action to perform");
    args.end = arg_end(1);

    int nerrors = arg_parse(argc, argv, (void **)&args);
    if (nerrors != 0) {
        arg_print_errors(stderr, args.end, argv[0]);
        return 1;
    }

    const char *action = args.action->sval[0];

    if (strcmp(action, "start") == 0) {
        ESP_LOGI("MEASUREMENT", "Starting measurement task...");
        hub_spawn_measurement_task();
    } else if (strcmp(action, "stop") == 0) {
        ESP_LOGI("MEASUREMENT", "Stopping measurement task...");
        hub_delete_measurement_task();
    } else {
        ESP_LOGW("MEASUREMENT", "Unknown action: %s", action);
        return 1;
    }

    return 0;
}

static int start_stats(int argc, char **argv) {
    static struct {
        struct arg_str *action;
        struct arg_end *end;
    } args;

    args.action = arg_str1(NULL, "action", "<start|stop>", "Action to perform");
    args.end = arg_end(1);

    int nerrors = arg_parse(argc, argv, (void **)&args);
    if (nerrors != 0) {
        arg_print_errors(stderr, args.end, argv[0]);
        return 1;
    }

    const char *action = args.action->sval[0];

    if (strcmp(action, "start") == 0) {
        ESP_LOGI("STATS", "Starting stats task...");
        stats_spawn_task();
    } else if (strcmp(action, "stop") == 0) {
        ESP_LOGI("STATS", "Stopping stats task...");
        stats_delete_task();
    } else {
        ESP_LOGW("STATS", "Unknown action: %s", action);
        return 1;
    }

    return 0;
}


static void register_mac_command(void)
{
    args.mac  = arg_str0(NULL, "mac", "<MAC>", "Target MAC address (e.g., AA:BB:CC:DD:EE:FF)");
    args.all  = arg_lit0(NULL, "all", "Send to all known peers");
    args.loop = arg_lit0(NULL, "loop", "Loop the command");
    args.end  = arg_end(3);

    argtable[0] = args.mac;
    argtable[1] = args.all;
    argtable[2] = args.loop;
    argtable[3] = args.end;

    const esp_console_cmd_t cmd = {
        .command = "sendcmd",
        .help = "Sends a command to initiate communication.",
        .hint = NULL,
        .func = &send_cmd_to_sta,
        .argtable = argtable
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}


static void register_measurement_command(void) {
    static struct {
        struct arg_str *action;
        struct arg_end *end;
    } args;

    args.action = arg_str1(NULL, "action", "<start|stop>", "Action to perform");
    args.end = arg_end(1);

    const esp_console_cmd_t cmd = {
        .command = "measurement",
        .help = "Measure data throughput and latency. Use --action start|stop",
        .hint = NULL,
        .func = &start_measurement,
        .argtable = &args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static void register_stats_command(void) {
    static struct {
        struct arg_str *action;
        struct arg_end *end;
    } args;

    args.action = arg_str1(NULL, "action", "<start|stop>", "Action to perform");
    args.end = arg_end(1);

    const esp_console_cmd_t cmd = {
        .command = "stats",
        .help = "Monitor tasks. Use --action start|stop",
        .hint = NULL,
        .func = &start_stats,
        .argtable = &args
    };

    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}



 /*******************************END: HELPER FUNCTION DEFINITIONS*******************************/
