**********START: Module customization and settings**********

--> idf.py menuconfig 
	--> Component config
		--> Hub Configuration
			--> Enable debug logging in HUB: The serial monitor will log debug information like messages sent and received.
			--> Station MAC addresses: It is possible to specify the peers in this submenu. You have to give the total number of peers and their MAC addresses.
		--> Stats Configuration 
			--> Enable FreeRTOS runtime stats using esp_timer: This must be enabled if you want to use task monitoring in serial monitor.

**********END: Module customization and settings**********

**********START: Available commands**********

--> help: Lists all available commands.
--> sendcmd: Sends a communication request to a peer or peers.
	--> Usage:
		--> sendcmd --mac=DD:DD:DD:DD:DD:DD: Sends a communication request to DD:DD:DD:DD:DD:DD
		--> sendcmd --mac=DD:DD:DD:DD:DD:DD --loop: Sends repeated communication requests to DD:DD:DD:DD:DD:DD
		--> sendcmd --all: Sends a communication request to all peers.
		--> sendcmd --all --loop: Sends repeated communication requests to all peers.
--> measurement: Logs information about the communication speed, latency and about the number of packets.
	--> Usage:
		--> measurement --action=start: Starts the measurement process.
		--> measurement --action=stop: Stops the measurement process.
--> stats: Logs information about the currently running tasks.
	--> Usage:
		--> stats --action=start: Starts the task monitoring process.
		--> stats --action=stop: Stops the task monitoring process.

**********END: Available commands**********