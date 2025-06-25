# ESP32-C6 Mesh Example (Arduino IDE) with painlessMesh

A simple 3-node mesh demo using [painlessMesh](https://github.com/gmag11/painlessMesh) on the ESP32-C6.

## Node Roles

- **Node 1**: Sends a broadcast message every 5 seconds
- **Node 2**: Relays messages through the mesh
- **Node 3**: Receives and prints messages to the Serial Monitor

## Setup Instructions

1. In the sketch, set the node role:
   ```cpp
   #define NODE_ROLE 1  // 1 = sender, 2 = relay, 3 = receiver


2. Configure the mesh network parameters:

   ```cpp
   #define MESH_PREFIX   "meshNetwork"
   #define MESH_PASSWORD "meshPassword"
   #define MESH_PORT     5555
   ```

3. Upload the sketch to each ESP32-C6 board with the appropriate `NODE_ROLE`.

4. Open the Serial Monitor at **115200 baud** to view output.

## Notes

* The mesh may become unstable with more than **4 devices** connected simultaneously.
* Maximum observed throughput in this configuration is approximately **400 kbps**.


