#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_wifi.h>
#include "ByteBuffer.h"

// ========== USER CONFIG ==========
const int FLEX_PINS[] = { 1, 2, 3, 4, 5 };   // 5 valid ADC GPIOs on ESP32-S3 ADAPT to your board.
const int NUM_SENSORS = 5;

const float VCC   = 3.3f;        // Sensor supply (V)
const float R_DIV = 10000.0f;    // Fixed resistor in divider (ohms)

// Invert (reverse) the flex data as required
const bool INVERT_FLEX = true;

// WiFi credentials
const char* ssid     = "your_ssid";
const char* password = "your_password";

// SlimeVR / UDP
IPAddress slimevrIp(255, 255, 255, 255);    // broadcast for discovery
const int slimevrPort = 6969;
const int protocolVersion = 19;             // first with flex + tracker position support
long packetId = 1;

WiFiUDP udp;
ByteBuffer packetBuffer;

// ========== SLIMEVR CONSTANTS ==========
const int IMU_TYPE_ID   = 1;  // placeholder (unused here)
const int BOARD_TYPE_ID = 2;  // SLIMEVR_DEV (example)
const int MCU_TYPE_ID   = 2;  // ESP32
const int DATA_SUPPORT_ID = 1; // FLEX_RESISTANCE
const float SMOOTH_FACTOR = 0.10f;

// Left/Right hand bone positions (as per SlimeVR)
const int BONE_POSITIONS_LEFT[]  = {22, 25, 28, 31, 34};
const int BONE_POSITIONS_RIGHT[] = {37, 40, 43, 46, 49};
// Choose which to use:
const int* BONE_POSITIONS = BONE_POSITIONS_LEFT;

// Smoothed resistance values (possibly inverted)
float currentResistance[NUM_SENSORS] = {0};

static inline float customLerp(float a, float b, float t) { return a + t * (b - a); }

static inline float safeDivide(float num, float den, float fallback) {
  return (den == 0.0f) ? fallback : (num / den);
}

// ========== PACKET SENDERS ==========
void sendHandshake() {
  packetBuffer.clear();
  packetBuffer.putInt(3);                  // Handshake
  packetBuffer.putLong(packetId++);        // Packet ID
  packetBuffer.putInt(BOARD_TYPE_ID);      // Board type
  packetBuffer.putInt(0);                  // IMU type (unused)
  packetBuffer.putInt(MCU_TYPE_ID);        // MCU type
  for (int i = 0; i < 3; i++) packetBuffer.putInt(0); // IMU info (unused)
  packetBuffer.putInt(protocolVersion);    // Protocol version

  String fwString = "GizmoGloves";
  packetBuffer.put((uint8_t)fwString.length());
  for (size_t i = 0; i < fwString.length(); i++) packetBuffer.put((uint8_t)fwString[i]);

  uint8_t mac[6];
  WiFi.macAddress(mac);
  for (int i = 0; i < 6; i++) packetBuffer.put(mac[i]);

  udp.beginPacket(slimevrIp, slimevrPort);
  udp.write(packetBuffer.array(), packetBuffer.size());
  udp.endPacket();
}

void sendSetupSensor(int trackerId) {
  packetBuffer.clear();
  packetBuffer.putInt(15);                 // Setup sensor
  packetBuffer.putLong(packetId++);        // Packet ID
  packetBuffer.put((uint8_t)trackerId);    // Tracker ID (firmware)
  packetBuffer.put((uint8_t)0);            // Sensor status
  packetBuffer.put((uint8_t)IMU_TYPE_ID);  // IMU type (placeholder)
  packetBuffer.putShort((uint16_t)0);      // Mag support
  packetBuffer.put((uint8_t)0);            // hasCompletedRestCalibration

  // Bone position + data type (flex resistance)
  packetBuffer.put((uint8_t)BONE_POSITIONS[trackerId]); // TrackerPosition
  packetBuffer.put((uint8_t)DATA_SUPPORT_ID);           // FLEX_RESISTANCE

  udp.beginPacket(slimevrIp, slimevrPort);
  udp.write(packetBuffer.array(), packetBuffer.size());
  udp.endPacket();
}

void sendFlexResistance(int sensorId, float resistance) {
  packetBuffer.clear();
  packetBuffer.putInt(26);                 // Flex resistance packet
  packetBuffer.putLong(packetId++);        // Packet ID
  packetBuffer.put((uint8_t)sensorId);     // Sensor ID (firmware)
  packetBuffer.putFloat(resistance);       // Ohms (smoothed, possibly inverted)

  udp.beginPacket(slimevrIp, slimevrPort);
  udp.write(packetBuffer.array(), packetBuffer.size());
  udp.endPacket();
}

void setup() {
  Serial.begin(115200);

  // Wi-Fi low-latency setup
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  // ADC init for ESP32-S3
  analogReadResolution(12); // 0..4095
  for (int i = 0; i < NUM_SENSORS; i++) {
    analogSetPinAttenuation(FLEX_PINS[i], ADC_11db); // ~0–3.3V range
    pinMode(FLEX_PINS[i], INPUT);
  }

  Serial.printf("Connecting to %s", ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.printf("\nWiFi connected. IP: %s\n", WiFi.localIP().toString().c_str());

  // UDP + protocol init
  udp.begin(slimevrPort);
  packetBuffer.init(128);
  sendHandshake();

  // Declare trackers and seed smoothing with first reading (already inverted)
  for (int i = 0; i < NUM_SENSORS; i++) {
    sendSetupSensor(i);

    int mv = analogReadMilliVolts(FLEX_PINS[i]); // calibrated mV
    if (mv < 5) mv = 5;                          // avoid 0 for division
    float v = mv / 1000.0f;                      // to volts
    float rflex = R_DIV * (safeDivide(VCC, v, 1.0f) - 1.0f);
    if (!isfinite(rflex) || rflex < 0) rflex = 0;
    if (INVERT_FLEX) rflex = -rflex;             // reverse here too
    currentResistance[i] = rflex;

    Serial.printf("Init sensor %d -> %.1f (ohms%s)\n",
                  i, currentResistance[i], INVERT_FLEX ? ", inverted" : "");
  }
}


void loop() {
  for (int i = 0; i < NUM_SENSORS; i++) {
    // Calibrated ADC read
    int mv = analogReadMilliVolts(FLEX_PINS[i]);
    if (mv < 5) mv = 5; // protect against 0 mV

    float v = mv / 1000.0f; // volts
    float rflex = R_DIV * (safeDivide(VCC, v, 1.0f) - 1.0f);

    // Sanity bounds
    if (!isfinite(rflex) || rflex < 0) rflex = 0;
    if (rflex > 1e6f) rflex = 1e6f;

    // Reverse (invert) before smoothing and sending
    if (INVERT_FLEX) rflex = -rflex;

    // Smooth and transmit
    currentResistance[i] = customLerp(currentResistance[i], rflex, SMOOTH_FACTOR);
    sendFlexResistance(i, currentResistance[i]);
  }

  delay(10); // ~100 Hz update rate
}
