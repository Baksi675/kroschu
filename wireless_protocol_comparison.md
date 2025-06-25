
---

# Wireless Protocol Comparison for ESP32-C6

This document summarizes the key specifications, capabilities, and limitations of various wireless protocols supported or usable with the ESP32-C6 and related ESP32 chips.

---
## Table of Contents
- [ESP-WiFi (802.11n)](#esp-wifi-80211n)  
- [ESP-NOW](#esp-now)  
  - [Measured Throughput – Default Mode (128-byte payload)(ESP-IDF)] 
  - [Measured Throughput – Default Mode (250-byte payload)(Arduino IDE)]
- [ESP-NOW Long Range Mode](#esp-now-long-range-mode)  
- [Zigbee (IEEE 802.15.4)](#zigbee-ieee-802154)  
- [PainlessMesh (Arduino Wi-Fi Mesh Library)](#painlessmesh-arduino-wi-fi-mesh-library)  
- [ESP-WiFi-Mesh (ESP-MDF)](#esp-wifi-mesh-esp-mdf) 

## ESP-WiFi (802.11n)

* **Max Throughput**: Up to \~150 Mbps (theoretical), \~<100 Mbps practical
* **Client Limit (SoftAP)**: Default 4, configurable up to \~8–10 clients
* **Range**:

  * Indoor: \~50–100 m
  * Outdoor: \~100–200 m (per hop)
* **Mesh**: Multi-hop supported via Wi-Fi Mesh frameworks

---

## ESP-NOW

* **Raw Throughput**: 1 Mbps (PHY), measured **\~200–550 kbps** depending on environment
* **Peer Limit**:

  * Up to 20 paired peers (unicast)
  * **Only 6 peers** if using encryption
  * Unlimited broadcast (custom peer list in payload)
* **Range**:

  * \~100–200 m outdoors
  * Shorter indoors
  * **Long Range Mode**: up to **\~1 km LOS** (with Espressif's LR mode)

### Measured Throughput – Default Mode (128-byte payload)(ESP-IDF):

Communication cycle:

```
HUB → CMD (4 B)  
STA → ACK (4 B)  
STA → DATA (128 B)  
```

| Distance | Throughput                 | Latency  |
| -------- | -------------------------- | -------- |
| 0 m      | \~220 kbps                 | 4800 µs  |
| 10 m     | \~220 kbps                 | 4800 µs  |
| 20 m     | \~215 kbps                 | 4900 µs  |
| 30 m     | \~215 kbps                 | 5100 µs  |
| 40 m     | \~150 kbps (variable)      | 7000 µs  |
| 50 m     | \~200 kbps (fluctuating)   | 6000 µs  |
| 60 m     | \~190 kbps (variable)      | 5700 µs  |
| 70 m     | \~180 kbps (variable)      | 5700 µs  |
| 80 m     | \~150 kbps (sometimes <50) | 8000 µs  |
| 90 m     | \~80 kbps (drops below 10) | 11000 µs |
| 100 m    | <10 kbps                   | 30000 µs |

 *Adjusting the antenna orientation significantly improves performance.*

### Measured Throughput – Default Mode (250-byte payload)(Arduino IDE):

### 1 cm - Antenna Facing Each Other

| Metric                         | Sender     | Receiver   |
| ------------------------------ | ---------- | ---------- |
| **Time (s)**                   | 30.0       | 31.0       |
| **Total Packets Sent / Valid** | 9 221      | 9 221      |
| **Payload Bytes (MB)**         | 2.30525 MB | 2.30525 MB |
| **Average Throughput (kbps)**  | 614.7 kbps | 594.9 kbps |
| **RSSI Avg (dBm)**             | —          | –1.4 dBm   |
| **Signal**                     | —          | 10 / 10    |
| **Corrupted Packets**          | —          | 0          |

### 1 cm - Antenna Opposing direction

| Metric                         | Sender     | Receiver   |
| ------------------------------ | ---------- | ---------- |
| **Time (s)**                   | 30.0       | 31.0       |
| **Total Packets Sent / Valid** | 9 181      | 9 181      |
| **Payload Bytes (MB)**         | 2.29525 MB | 2.29525 MB |
| **Average Throughput (kbps)**  | 612.0 kbps | 592.3 kbps |
| **RSSI Avg (dBm)**             | —          | –18.5 dBm  |
| **Signal**                     | —          | 10 / 10    |
| **Corrupted Packets**          | —          | 0          |

### 1.5m - Antenna Facing Each Other

| Metric                         | Sender     | Receiver   |
| ------------------------------ | ---------- | ---------- |
| **Time (s)**                   | 30.0       | 31.0       |
| **Total Packets Sent / Valid** | 8 845      | 8 845      |
| **Payload Bytes (MB)**         | 2.21125 MB | 2.21125 MB |
| **Average Throughput (kbps)**  | 589.6 kbps | 570.6 kbps |
| **RSSI Avg (dBm)**             | —          | –40.2 dBm  |
| **Signal**                     | —          | 8 / 10     |
| **Corrupted Packets**          | —          | 0          |

### 17m - Antenna Facing Each Other

| Metric                         | Sender     | Receiver   |
| ------------------------------ | ---------- | ---------- |
| **Time (s)**                   | 30.0       | 31.0       |
| **Total Packets Sent / Valid** | 8 652      | 8 652      |
| **Payload Bytes (MB)**         | 2.16300 MB | 2.16300 MB |
| **Average Throughput (kbps)**  | 576.8 kbps | 558.2 kbps |
| **RSSI Avg (dBm)**             | —          | –66.4 dBm  |
| **Signal**                     | —          | 4 / 10     |
| **Corrupted Packets**          | —          | 0          |

### 34m – Antenna Facing Each Other

| Metric                         | Sender     | Receiver   |
| ------------------------------ | ---------- | ---------- |
| **Time (s)**                   | 30.0       | 31.0       |
| **Total Packets Sent / Valid** | 7 885      | 7 885      |
| **Payload Bytes (MB)**         | 1.97125 MB | 1.97125 MB |
| **Average Throughput (kbps)**  | 525.6 kbps | 508.7 kbps |
| **RSSI Avg (dBm)**             | —          | –63.8 dBm  |
| **Signal**                     | —          | 5 / 10     |
| **Corrupted Packets**          | —          | 0          |

### 34m – Antenna Opposing Direction

| Metric                         | Sender     | Receiver   |
| ------------------------------ | ---------- | ---------- |
| **Time (s)**                   | 30.0       | 31.0       |
| **Total Packets Sent / Valid** | 7 896      | 7 896      |
| **Payload Bytes (MB)**         | 1.97400 MB | 1.97400 MB |
| **Average Throughput (kbps)**  | 526.4 kbps | 509.4 kbps |
| **RSSI Avg (dBm)**             | —          | –72.0 dBm  |
| **Signal**                     | —          | 4 / 10     |
| **Corrupted Packets**          | —          | 0          |


---

## ESP-NOW Long Range Mode

* **Improvements**: \~4 dB gain in sensitivity over default
* **Range**:

  * Up to \~1 km (Espressif lab tests)
  * \~650 m real-world tested on ESP32-S3
* **Pairing Rules**: Same as ESP-NOW (20 peers max, 6 encrypted)
* **Latency and Throughput**: Comparable to default ESP-NOW mode

---

## Zigbee (IEEE 802.15.4)

* **Max Throughput**: \~250 kbps (real-world: <100 kbps due to 6LoWPAN + IP stack overhead)
* **Range**: \~10–100 m per hop
* **Device Capacity**:

  * Thread/Zigbee spec: up to 16,384 devices (theoretical)
  * Real-world: few hundred stable devices
* **Features**:

  * Self-healing mesh
  * IPv6 support via 6LoWPAN
  * Low power, ideal for sensors and smart home applications

---

## PainlessMesh (Arduino Wi-Fi Mesh Library)

* **Throughput**:

  * Raw Wi-Fi limits apply (802.11n)
  * Few Mbps throughput in small networks
  * Observed throughput: 400kbps
  * Drops sharply with more hops
* **Range**:

  * Per hop: \~10–50 m indoors
  * Extended with multi-hop
* **Device Limit**:

  * No strict cap; constrained by device memory
  * Practical stable networks: few dozen nodes
  * Large networks (>100) reported unstable

---

## ESP-WiFi-Mesh (ESP-MDF)

> ⚠️ **Deprecated**

* **Framework**: ESP-MDF (Espressif's Mesh Development Framework)
* **Router Dependency**:

  * Originally supported routerless mesh
  * Now requires router for setup and communication
* **Status**:

  * Development ended in **2021**
  * GitHub repository officially **archived on June 17, 2025**
* **Recommendation**: Avoid for new projects

---
