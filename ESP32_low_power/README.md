# ESP-NOW Power Consumption Test on ESP32-C6

This repository provides two standalone examples for measuring power consumption of an ESP32-C6 in various power modes using ESP-NOW communication. The **Sender** transmits a small payload (boot count and timestamp) and then enters a deep-sleep cycle. The **Receiver** continuously listens for incoming ESP-NOW packets and logs the data.

## Repository Structure

```text
├── sender_sleep/        # Sender example
│   ├── CMakeLists.txt
│   ├── README.md
│   └── main/            # Application source directory
│       ├── CMakeLists.txt
│       └── main.c       # Sends data then sleeps
└── peer/                # Receiver example
    ├── CMakeLists.txt
    ├── README.md
    └── main/            # Application source directory
        ├── CMakeLists.txt
        └── main.c       # Logs incoming data
```
<br>

## Power Mode Measurement Results (ESP32-C6)


| Mode                     | Current Draw | Notes                                                                                        |
| ------------------------ | ------------ | -------------------------------------------------------------------------------------------- |
| **No Wi-Fi**             |              | CPU idle, Wi-Fi off                                                                          |
| • CPU @ 160 MHz          | \~30 mA      |                                                                                              |
| • CPU @ 120 MHz          | \~28 mA      |                                                                                              |
| • CPU @  80 MHz          | \~24 mA      |                                                                                              |
| **ESP-NOW Active**       | \~100 mA     | Wi-Fi/ESP-NOW idle                                                                           |
| **ESP-NOW Transmission** | \~220 mA     | During packet TX                                                                             |
| **Modem-sleep**          | \~80–100 mA  | `esp_wifi_set_ps(WIFI_PS_MIN_MODEM)` → CPU, RAM, peripherals active                          |
| **Light-sleep**          | \~1 mA       | `esp_light_sleep_start()` → CPU, RAM, peripherals clock-gated, state retained                |
| **Deep-sleep**           | \~10 µA      | `esp_deep_sleep_start()` → CPU, RAM off; RTC controller, ULP, RTC FAST memory remain powered |
| **Hibernation**          | \~5 µA       | Lowest-power; retains only RTC timer and select RTC GPIOs                                    |

<br>
<br>

 **Modem-sleep:** Disables radio modules (Wi-Fi/Bluetooth) but keeps CPU, RAM, and peripherals running. Power draw depends on the selected CPU frequency (does not correspond with measured values).

 **Light-sleep:** Clocks gated for CPU, RAM, and digital peripherals; state retained. On wake, execution resumes where it left off.

 **Deep-sleep:** Powers off CPU, RAM, and digital peripherals; only RTC controller, ULP coprocessor, and RTC FAST memory remain powered. Wake triggers a cold boot, retaining only RTC memory data.

 **Hibernation:** All oscillators, RTC domains, and memories disabled except a single RTC timer and a few RTC GPIOs. No context is retained.




