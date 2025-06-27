/*******************************BEGIN: PURPOSE OF MODULE*******************************
 * ESP32‑C6 Receiver for throughput, RSSI & CRC testing over ESP‑NOW.
 * – Validates incoming packets (CRC32 over 250 B payload).
 * – Tracks total/periodic bytes, packet count, corruption count.
 * – Computes and reports per‑second throughput and RSSI quality.
 *******************************END: PURPOSE OF MODULE*******************************/

#include <esp_now.h>
#include <WiFi.h>
#include "esp_crc.h"   // for esp_crc32_le()

/*******************************BEGIN: CONFIGURATION PARAMETERS**********************
 * REPORT_INTERVAL_MS – ms between stats reports
 *******************************END: CONFIGURATION PARAMETERS*************************/
#define REPORT_INTERVAL_MS 1000

/*******************************BEGIN: DATA STRUCTURES*********************************/
/* struct_message
 * – seq     : 32‑bit sequence number
 * – payload : 250 bytes of data
 * – crc     : CRC32 over payload[]
 */
typedef struct {
  uint32_t seq;
  uint8_t  payload[250];
  uint32_t crc;
} struct_message;
/*******************************END: DATA STRUCTURES***********************************/

/*******************************BEGIN: GLOBAL VARIABLES********************************/
struct_message  incomingData;
int              lastSeq           = 0;
unsigned long    startTime         = 0;
unsigned long    lastReport        = 0;
uint32_t         bytesThisPeriod   = 0;
uint32_t         totalBytes        = 0;
uint32_t         totalPackets      = 0;
uint32_t         totalCorrupted    = 0;
int32_t          rssiSumPeriod     = 0;
uint32_t         rssiCountPeriod   = 0;
/*******************************END: GLOBAL VARIABLES**********************************/

/*******************************BEGIN: CALLBACK DEFINITION******************************/
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  // Validate packet length
  if (len != sizeof(struct_message)) {
    Serial.printf("[RX] Unexpected size: %d (expected %d)\n", len, sizeof(struct_message));
    return;
  }

  // Deserialize and CRC check
  memcpy(&incomingData, data, sizeof(incomingData));
  uint32_t crcCalc = esp_crc32_le(0, incomingData.payload, sizeof(incomingData.payload));
  if (crcCalc != incomingData.crc) {
    totalCorrupted++;
    Serial.printf("[RX] Seq %5u | CRC MISMATCH (got 0x%08X, exp 0x%08X)\n",
                  incomingData.seq, incomingData.crc, crcCalc);
    return;
  }

  // RSSI accumulation
  int8_t rssi = info->rx_ctrl->rssi;
  rssiSumPeriod  += rssi;
  rssiCountPeriod++;

  // Throughput counters
  totalPackets++;
  totalBytes      += sizeof(incomingData.payload);
  bytesThisPeriod += sizeof(incomingData.payload);

  // First-packet timestamp
  if (totalPackets == 1) {
    startTime  = millis();
    lastReport = startTime;
    Serial.println(">>> Starting data reception (CRC OK)...");
  }

  // Detect sequence gaps
  if (lastSeq != 0 && incomingData.seq != lastSeq + 1) {
    Serial.printf("[RX] Packet gap: expected %u, got %u\n", lastSeq + 1, incomingData.seq);
  }
  lastSeq = incomingData.seq;
}
/*******************************END: CALLBACK DEFINITION*******************************/

/*******************************BEGIN: SETUP FUNCTION**********************************/
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== ESP32‑C6 Receiver: Throughput + RSSI + CRC Test ===");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  Serial.print("Receiver MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.println("Waiting for incoming ESP‑NOW packets...\n");

  // Init ESP‑NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error: ESP‑NOW init failed");
    while (true) delay(1000);
  }
  esp_now_register_recv_cb(OnDataRecv);
}
/*******************************END: SETUP FUNCTION************************************/

/*******************************BEGIN: MAIN LOOP FUNCTION*******************************/
void loop() {
  unsigned long now = millis();

  // Report stats every REPORT_INTERVAL_MS after first packet
  if (totalPackets > 0 && now - lastReport >= REPORT_INTERVAL_MS) {
    float elapsedSec = (now - startTime) / 1000.0f;
    float avgKbps    = (totalBytes * 8.0f) / (elapsedSec * 1000.0f);
    float curKbps    = (bytesThisPeriod * 8.0f) / (REPORT_INTERVAL_MS / 1000.0f) / 1000.0f;

    float avgRSSI = rssiCountPeriod
                   ? (float)rssiSumPeriod / rssiCountPeriod
                   : 0.0f;
    int quality = constrain(map((int)avgRSSI, -100, -30, 0, 10), 0, 10);

    Serial.printf(
      "Time: %5.1f s | Packets: %u | Bytes: %u | Avg: %.1f kbps | Cur: %.1f kbps\n"
      "RSSI: %5.1f dBm | Quality: %d/10 | Corrupt: %u\n\n",
      elapsedSec, totalPackets, totalBytes, avgKbps, curKbps,
      avgRSSI, quality, totalCorrupted
    );

    // Reset period counters
    bytesThisPeriod = 0;
    rssiSumPeriod   = 0;
    rssiCountPeriod = 0;
    lastReport      = now;
  }
}
/*******************************END: MAIN LOOP FUNCTION*******************************/


