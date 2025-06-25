#include <painlessMesh.h>

// ─── Node Role Configuration ────────────────────────────────────────────────────
// Set to 1 = sender, 2 = relay, or 3 = receiver
#define NODE_ROLE     1

// ─── Mesh Network Settings ─────────────────────────────────────────────────────
#define MESH_PREFIX   "meshNetwork"
#define MESH_PASSWORD "meshPassword"
#define MESH_PORT     5555

Scheduler    userScheduler;  // task scheduler
painlessMesh mesh;           // mesh object

// ─── Function Declarations ─────────────────────────────────────────────────────
void sendMessageTask();
void receivedCallback(uint32_t from, String &msg);

// ─── Task: Send Message Every 5s ───────────────────────────────────────────────
Task taskSendMessage(5000, TASK_FOREVER, &sendMessageTask);

// ─── Incoming Message Handler ──────────────────────────────────────────────────
void receivedCallback(uint32_t from, String &msg) {
  if (NODE_ROLE == 3) {
    Serial.printf("Node 3 got from %u: %s\n", from, msg.c_str());
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  mesh.setDebugMsgTypes(ERROR | STARTUP | CONNECTION);
  mesh.init(MESH_PREFIX, MESH_PASSWORD, MESH_PORT);
  mesh.onReceive(&receivedCallback);

  // If sender, start the periodic task
  if (NODE_ROLE == 1) {
    userScheduler.addTask(taskSendMessage);
    taskSendMessage.enable();
  }
}

void loop() {
  mesh.update();
  userScheduler.execute();
}

// ─── sendMessageTask ──────────────────────────────────────────────────────────
// Only runs on NODE_ROLE == 1
void sendMessageTask() {
  if (NODE_ROLE == 1) {
    String msg = "Hello from Node 1";
    mesh.sendBroadcast(msg);
    Serial.println("Node 1 sent: " + msg);
  }
}

