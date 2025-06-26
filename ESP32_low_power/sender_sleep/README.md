# ESP-NOW Deep-Sleep Sender Example

This example demonstrates how to use ESP-NOW on an ESP32-C6 to send a small data packet and then enter deep sleep to minimize power consumption.

## Features
- Tracks boot count across wake cycles
- Sends a payload containing boot count and timestamp
- Enters deep sleep for configurable duration

## Hardware
- ESP32-C6 dev board

## Software Requirements
- ESP-IDF 5.1+ with ESP32-C6 support
- VSCode with ESP-IDF Extension

## Configuration
- Adjust `TIME_TO_SLEEP` in `main.c` (default: 30 seconds)
- Update the receiver MAC address in `peerAddress[]`