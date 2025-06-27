/*******************************BEGIN: PURPOSE OF MODULE*******************************
 * ESP32‑C6 Sender for throughput & CRC testing over ESP‑NOW.
 * – Sends 250 B random‑data packets for a fixed duration.
 * – Appends CRC32 for integrity check.
 * – Reports interim and final throughput stats.
 *******************************END: PURPOSE OF MODULE*******************************/

#include <esp_now.h>
#include <WiFi.h>
#include "esp_crc.h"   // for esp_crc32_le()

/*******************************BEGIN: CONFIGURATION PARAMETERS**********************
 * TEST_DURATION_MS    – total test time (ms)
 * REPORT_INTERVAL_MS  – ms between interim reports
 *******************************END: CONFIGURATION PARAMETERS***********************/
#define TEST_DURATION_MS   30000
#define REPORT_INTERVAL_MS 1000

/*******************************BEGIN: DATA STRUCTURES*********************************/
/* struct_message
 * – seq     : 32‑bit sequence number
 * – payload : 250 bytes of random data
 * – crc     : CRC32 over payload[]
 */
typedef struct {
  uint32_t seq;
  uint8_t  payload[250];
  uint32_t crc;
} struct_message;
/*******************************END: DATA STRUCTURES***********************************/

/*******************************BEGIN: GLOBAL VARIABLES********************************/
uint8_t         receiverAddress[] = {0x54,0x32,0x04,0x07,0x41,0xF4};
struct_message  packetToSend;
esp_now_peer_info_t peerInfo;
unsigned long   startTime       = 0;
unsigned long   lastReport      = 0;
uint32_t        totalBytesSent  = 0;
uint32_t        totalPacketsSent= 0;
/*******************************END: GLOBAL VARIABLES**********************************/

/*******************************BEGIN: CALLBACK DEFINITION******************************/
/* OnDataSent()
 * – Stub: we ignore send failures in byte count.
 */
void OnDataSent(const uint8_t *mac, esp_now_send_status_t status) { }
/*******************************END: CALLBACK DEFINITION*******************************/

/*******************************BEGIN: SETUP FUNCTION**********************************/
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== ESP32‑C6 Sender: Throughput + CRC Test ===");
  Serial.print("Receiver MAC: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X%s", receiverAddress[i], i<5?":":"");
  }
  Serial.println("\n");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error: ESP‑NOW init failed");
    while (true) delay(1000);
  }
  esp_now_register_send_cb(OnDataSent);

  memcpy(peerInfo.peer_addr, receiverAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Error: Add peer failed");
    while (true) delay(1000);
  }

  randomSeed(analogRead(0));
  for (int i = 0; i < 250; i++) packetToSend.payload[i] = random(0,256);

  delay(1000);
  Serial.println("Starting throughput test in 2 seconds...");
  delay(2000);

  startTime      = millis();
  lastReport     = startTime;
  totalBytesSent = 0;
  totalPacketsSent = 0;
}
/*******************************END: SETUP FUNCTION************************************/

/*******************************BEGIN: MAIN LOOP FUNCTION*******************************/
void loop() {
  unsigned long now = millis();

  if (now - startTime <= TEST_DURATION_MS) {
    // Prepare and send next packet
    packetToSend.seq = totalPacketsSent + 1;
    packetToSend.crc = esp_crc32_le(0, packetToSend.payload, sizeof(packetToSend.payload));

    esp_err_t res = esp_now_send(receiverAddress,
                                 (uint8_t *)&packetToSend,
                                 sizeof(packetToSend));
    if (res == ESP_OK) {
      totalPacketsSent++;
      totalBytesSent += sizeof(packetToSend.payload);
    } else {
      Serial.printf("[SEND FAIL] Seq:%u Err:%d (%s)\n",
                    packetToSend.seq, res, esp_err_to_name(res));
    }
    delay(5);

    // Interim stats
    if (now - lastReport >= REPORT_INTERVAL_MS) {
      float secs = (now - startTime) / 1000.0f;
      float kbps = (totalBytesSent * 8.0f) / (secs * 1000.0f);
      Serial.printf("Time: %5.1f s | Packets: %u | Bytes: %u | Throughput: %.1f kbps\n",
                    secs, totalPacketsSent, totalBytesSent, kbps);
      lastReport = now;
    }

  } else {
    // Final report and halt
    float totalSec = (now - startTime) / 1000.0f;
    float finalK = (totalBytesSent * 8.0f) / (totalSec * 1000.0f);
    Serial.println("\n=== TEST COMPLETE ===");
    Serial.printf("Duration: %.2f s | Packets: %u | Bytes: %u | Avg: %.1f kbps\n",
                  totalSec, totalPacketsSent, totalBytesSent, finalK);
    Serial.println("Power-cycle or reset to run again.");
    while (true) delay(1000);
  }
}
/*******************************END: MAIN LOOP FUNCTION*******************************/
