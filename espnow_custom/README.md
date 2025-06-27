# Module customization and settings

## Configuration via `idf.py menuconfig`

### Component config → Select Hub / Station Mode
- **Hub Mode**:
  Hub Mde will be enabled.
- **Station Mode**:
  Station mode will be enabled.
### Component config → Hub Configuration
- **Enable debug logging in HUB**:  
  The serial monitor will log debug information like messages sent and received.
- **Station MAC addresses**:  
  It is possible to specify the peers in this submenu. You have to give:
  - The total number of peers
  - Their MAC addresses
### Component config → Station Configuration
- **Enable debug logging in STA**:
  The serial monitor will log debug information.
- **Hub MAC address**:
  You can specify the Hub MAC address here.
### Component config → Stats Configuration
- **Enable FreeRTOS runtime stats using esp_timer**:  
  This must be enabled if you want to use task monitoring in serial monitor.

---

# Available commands in the serial terminal

## `help`
Lists all available commands.

## `sendcmd`
Sends a communication request to a peer or peers.

### Usage:
- `sendcmd --mac=DD:DD:DD:DD:DD:DD`  
  Sends a communication request to DD:DD:DD:DD:DD:DD
- `sendcmd --mac=DD:DD:DD:DD:DD:DD --loop`  
  Sends repeated communication requests to DD:DD:DD:DD:DD:DD
- `sendcmd --all`  
  Sends a communication request to all peers
- `sendcmd --all --loop`  
  Sends repeated communication requests to all peers

## `measurement`
Logs information about the communication speed, latency and about the number of packets.

### Usage:
- `measurement --action=start`  
  Starts the measurement process
- `measurement --action=stop`  
  Stops the measurement process

## `stats`
Logs information about the currently running tasks.

### Usage:
- `stats --action=start`  
  Starts the task monitoring process
- `stats --action=stop`  
  Stops the task monitoring process

# Software explanation

## *****hub module*****
This module implements an ESP32 HUB that serves as the central communication node in an ESP-NOW network. The module exposes several APIs that make it possible to use the functionalities of the module from the outside code.

### Functions

#### `void hub_init(void)`
This API initializes all the necessary built-in components required for creating an ESP-NOW communication node. It also initializes the self-created hub module. These include:
- `nvs_flash_init()` ==> Initializes the default NVS (Non Volatile Storage) partition, required for many components (Wi-Fi, Bluetooth, ESP-NOW) to store and retrieve settings in / from the non-volatile storage.
- `esp_netif_init()` ==> It's used to initialize the networking stack (NETIF), required for network interfaces.
- `esp_event_loop_create_default()` ==> Handles system events (eg. Wi-Fi connection events, ESP-NOW events). Internally used by Wi-Fi and ESP-NOW components. Can also be used to handle events like Wi-Fi disconnected, package sent etc.
- `esp_wifi_init()` ==> Initializes the Wi-Fi driver (with a configuration structure passed as parameter, the structure is initialized to default configuration)
- `esp_wifi_set_mode()` ==> Sets the Wi-Fi mode, in this case to STATION
- `esp_wifi_start()` ==> Starts the Wi-Fi driver
- `esp_wifi_set_ps()` ==>
- `esp_wifi_set_channel()` ==> Specifies to Wi-Fi channel to be used
- `esp_now_init()` ==> Initializes ESP-NOW
- `esp_now_register_send_cb()` ==> Registers a callback function for send events
- `esp_now_register_recv_cb()` ==> Registers a callback function for receive events

The API then proceeds to create two Queues for communication between the callbacks and tasks. Both Queues have a size of 1 to minimize complexity. The send callback queue contains data of type `esp_now_send_status_t`, which indicates if the sent message was received successfuly by the physical layer of the receiver. The receive callback queue contains data of the self-created type `RECEIVE_DATA_t`, which contains the received message length and the message itself. The API also creates a mutex semaphore for ensuring that no race-condition occurs when the receive callback and the running tasks access the speed measurement data. There is also a binary semaphore created for signaling if a communication task is allowed to continue running. At the end of the initialization API, the peer array initialization function is called and the device MAC address is printed on the serial terminal.

#### `void hub_spawn_comm_task(PEER_t peer)`
This API is responsible for creating a communication task with the peer that has been passed as an argument. `PEER_t` is a self-created type which contains an array of type `uint8_t` with a size of 6 (size of a MAC address). The API dynamically allocates memory for the peer obtained via the parameter. This is necessary because this APIs sole purpose is spawning a task, after which it will be destroyed and the stack it held previously will be freed. This can be solved via dynamically allocating memory for the peer and passing its pointer to the created task. This way the task can work with the peer data, altough the allocated memory will have to be freed manually by the task.

#### `void hub_spawn_comm_loop_task(PEER_t peer)`
This API is responsible for creating a looping communication task with the peer thas has been passed as an argument. The API works the same way as `hub_spawn_comm_task()` API does.

#### `void hub_spawn_comm_all_loop_task(int loop)`
This API is responsible for creating a communication task between all peers in the peer list. Looping can be specified in the parameter. The API works the same way as `hub_spawn_comm_task()` and `hub_spawn_comm_loop_task()` do.

#### `void hub_spawn_measurement_task(void)`
This API spawns a measurement task.

#### `void hub_delete_measurement_task(void)`
This API deletes a measurement task.

#### `static void hub_espnow_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)`
This callback get called whenever a packet has been sent with ESP-NOW protocol. Its arguments contain information about the data, the source, destination etc. and also about the status of the send procedure. The purpose of this callback is to transfer the status of a send (success or not) to certain tasks. This can be accomplished with using a queue.
The callback acts as an interrupt service routine (ISR) which has to be kept in mind. ISRs must be as short as possible and they should not block execution at any given time to avoid system latency issues. That's why the specific `xQueueSendFromISR` call is used to instert the status item into a queue. There is also a `xHigherPriorityTaskWoken` flag used, which indicates if a task has been unblocked as a result of inserting the item into the queue. If this is the case, the control is immediately given up by the ISR and context switch happens (so a higher priority task can run immediately).

#### `static void hub_espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)`
The purpose of this callback is to pass the received data into a queue that can be taken out by a task and processed. First the callback adds the length ot the packet to a global variable, that is used for measurement purposes. Then the callback inserts the data and it's length into a queue.

#### `static void hub_connect_peer(PEER_t g_peer)`
This function is used to add the peer passed by parameter to the peer list.

#### `static void hub_disconnect_peer(PEER_t g_peer)`
This function is used to delete the peer passed by parameter from the peer list.

#### `static void hub_print_mac_addr(void)`
This function is used to print the device MAC address on the serial terminal.

#### `static void hub_peer_arr_init(void)`
This function initializes the array containing the peer MAC addresses. The MAC addresses are entered from menuconfig so parsing is done by this function.

#### `static void hub_comm_task(void *arg)`
This task handles the communication with a station. A pointer to the peer in memory is received via parameter, which is then copied by value then the memory is freed. The task then adds the peer to the peer list by calling `hub_connect_peer()`. The task first send a command to the station to request data. After the command has been sent the task checks for acknowledgment sent by the station. If acknowledgement was received successfuly, data can be accepted by the hub. After the communication cycle completes, the task deletes itself.

#### `static void hub_comm_loop_task(void *arg)`
This task repeatedly spawns communication tasks after they have finished their communication cycles. The peer is passed by parameter.

#### `static void hub_comm_all_task(void *arg)`
This task spawns communication tasks to all peers. Looping can be specified in the parameter.

#### `static void hub_speed_measurement_task(void *arg)`
This task measures the speed, latency and packets sent under 1 second.

## *****station module*****
This module implements an ESP32 STATION that serves as a slave node in an ESP-NOW network 

### Functions

#### `void sta_init(void)`
This API initializes all the necessary built-in components required for creating an ESP-NOW communication node. It also initializes the self-created sta module.

#### `static void sta_espnow_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)`
Works the same way as the send callback function in the hub module.

#### `static void sta_espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)`
Works the same way as the receive callback function in the hub module.

#### `static void sta_connect_peer(PEER_t g_peer)`
Works the same way as the `hub_conncet_peer()` function in the hub module.

#### `static void sta_init_tasks(void)`
Spawns a communication task with the hub.

#### `static void sta_communication_task(void *arg)`
The task receives a command for communication from a hub, then it sends out an acknowledgment after which it immediately sends the data. Then is starts listening for a command once again (task deletion is not implemented). 

#### `static void sta_print_mac_addr(void)`
Prints the MAC address of the device.

## *****stats module*****
The stats module is a modified version of the ESP-IDF example code of real_time_stats. This module repeatedly prints out the currently running tasks on the serial monitor with a 1 second interval. The module has two important APIs.

### Functions

#### `void stats_spawn_task(void)`
This API spawns a stats monitoring task.

#### `void stats_delete_task(void)`
This API deletes the stats monitoring task.

#### `void stats_task(void *arg)`
This task uses the `print_real_time_stats()` function of the example code, which prints out the currently running tasks and the CPU time they consume. The task is delayed for 1 second, then repeatedly runs again.

## *****cconsole module*****
This module implements a console system, that can be used for debugging, monitoring and run-time communication with the device. The module is a modification of the ESP-IDF example with the name of console (basic).

### Functions

#### `void cconsole_init(enum e_device_mode device_mode)`
This API initializes the ESP console, sets up its prompt and history, registers available commands based on device mode, and starts an interactive REPL over the selected transport (UART, USB CDC, or USB Serial JTAG).

#### `static void initialize_nvs(void)`
This function initializes the non-volatile storage.

#### `static int start_sendcmd(int argc, char **argv)`
This function gets called on a command sent by a hub device. Handles the arguments of the command and calls the necessary APIs exposed by the hub module. (this command is only available for a hub device)

#### `static int start_measurement(int argc, char **argv)`
This function gets called on a measurement command. Handles the arguments of the command and calls the necessary APIs exposed by the hub module. (this command is only available for a hub device)

#### `static int start_stats(int argc, char **argv)`
This function gets called on a stats command. Handles the arguments of the command and calls the necessary APIs exposed by the stats module. (this command is available for both hub and station devices)

#### `static void register_sendcmd_command(void)`
This function registers a command for sending out a communication request to a peer.

#### `static void register_measurement_command(void)`
This function registers a command for measuring the speed, latency and number of packets in a communication cycle.

#### `static void register_stats_command(void)`
This function registers a command for monitoring RTOS tasks in the system.





