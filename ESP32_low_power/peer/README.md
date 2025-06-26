# ESP-NOW Receiver Example

This example configures an ESP32-C6 to receive ESP-NOW packets, optionally filter by sender MAC, and log the received payload.

## Features
- Receives boot count and timestamp payloads
- Optional MAC filtering
- Low-power modem-sleep between packets

## Hardware
- ESP32-C6 dev board

## Software Requirements
- ESP-IDF 5.1+ with ESP32-C6 support
- VSCode with ESP-IDF Extension

## Configuration
- Update the sender MAC address in `senderAddress[]` (use broadcast `FF:FF:FF:FF:FF:FF` for all)