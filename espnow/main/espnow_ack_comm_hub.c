#include "hub.h"
#include "station.h"
#include "cconsole.h"
#include "sdkconfig.h"

void app_main(void)
{
#if CONFIG_HUB_MODE
	hub_init();
	cconsole_init(MODE_HUB);
#endif
#if CONFIG_STATION_MODE
	sta_init();
	cconsole_init(MODE_STA);
#endif
}


