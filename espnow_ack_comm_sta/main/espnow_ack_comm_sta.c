/*
	Standard library, header file includes
*/
#include "station.h"
#include "stats.h"

/*
	Helper function prototypes
*/

/*
	Main function
*/
void app_main(void)
{
	stats_start();
	sta_init();
}

/*
	A helper function to initialize tasks
*/
