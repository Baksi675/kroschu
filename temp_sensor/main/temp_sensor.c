#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "rom/ets_sys.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_wifi.h"
#include "esp_event.h"


#include "DHT22.h"

void DHT_reader_task(void *pvParameter)
{
       setDHTgpio(GPIO_NUM_0);
		

	while(1) {
	
		printf("DHT Sensor Readings\n" );
		int ret = readDHT();
		
		errorHandler(ret);

		printf("Humidity %.2f %%\n", getHumidity());
		printf("Temperature %.2f degC\n\n", getTemperature());
		
		vTaskDelay(pdMS_TO_TICKS(2000));
	}
}

void app_main()
{
    nvs_flash_init();
    vTaskDelay(pdMS_TO_TICKS(2000));
    xTaskCreate(&DHT_reader_task, "DHT_reader_task", 2048, NULL, 5, NULL );
}

