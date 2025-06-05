#include <stdio.h>

#include "driver/gpio.h"
#include "freertos/projdefs.h"
#include "hal/gpio_types.h"
#include "soc/gpio_num.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

void InitLed(void);
void LedRun(void);

void app_main(void)
{
	InitLed();

	while(1) {
		LedRun();
	}
}

void InitLed(void) {
	gpio_set_direction(GPIO_NUM_2,  GPIO_MODE_OUTPUT);
}

void LedRun(void) {
	gpio_set_level(GPIO_NUM_2, 1);
	vTaskDelay(pdMS_TO_TICKS(1000));
	ESP_LOGI("LED", "LED ON/OFF STATUS", gpio_get_level(GPIO_NUM_2));
	gpio_set_level(GPIO_NUM_2, 0);
	vTaskDelay(pdMS_TO_TICKS(1000));
	ESP_LOGI("LED", "LED ON/OFF STATUS", gpio_get_level(GPIO_NUM_2));
}