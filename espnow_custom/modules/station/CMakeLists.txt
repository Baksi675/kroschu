idf_component_register(SRCS "station.c" INCLUDE_DIRS "."
	REQUIRES esp_event esp_wifi nvs_flash esp_netif
	)
