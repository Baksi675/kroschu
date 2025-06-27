
# ESP32-C6 Arduino IDE ESP-Now Throughput & CRC Test

Compact demo for sender/receiver testing over ESP-Now.

## Components:

- sender.ino: Sends 250-byte random data packets with CRC32 for a fixed duration.
- receiver.ino: Validates packets using CRC, tracks throughput and RSSI, and reports stats every second.

## Quick Start:

1. Clone the repository and open both sketches in the Arduino IDE.
2. In sender.ino, set the target MAC address:
   uint8_t receiverAddress[] = { 0x54, 0x32, 0x04, 0x07, 0x41, 0xF4 };
3. Upload receiver.ino to the receiver board, then sender.ino to the sender board.
4. Open Serial Monitors at 115200 baud on both devices.

## Configurable Parameters:

#define TEST_DURATION_MS   30000  // test length in milliseconds
#define REPORT_INTERVAL_MS 1000   // sender report interval
#define REPORT_INTERVAL_MS 1000   // receiver report interval

## Notes:

- Unstable when distance increases. If you encounter ESP_ERR_ESPNOW_NO_MEM (out of memory), try increasing the delay before sending the next packet.
- Maximum observed throughput is around 400 Kbps.
