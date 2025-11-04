#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_wifi.h>
#include "ByteBuffer.h"

// ========== USER CONFIG ==========
const int FLEX_PINS[] = { 1, 2, 3, 4, 5 };   // 5 valid ADC GPIOs on ESP32-S3 (GPIO numbers). ADAPT to your board.
const int NUM_SENSORS = 5;

const float VCC   = 3.3f;        // Sensor supply (V)
const float R_DIV = 10000.0f;    // Fixed resistor in divider (ohms)

// Invert (reverse) the flex data
const bool INVERT_FLEX = true;

// WiFi
const char* ssid     = "your_ssid";
const char* password = "your_password";

// SlimeVR / UDP
IPAddress slimevrIp(255, 255, 255, 255);    // broadcast for discovery
const int slimevrPort = 6969;
const int protocolVersion = 19;             // first with flex + tracker position support
long packetId = 1;

WiFiUDP udp;
ByteBuffer packetBuffer;


const int IMU_TYPE_ID   = 1;  // placeholder (unused here)
const int BOARD_TYPE_ID = 2;  // SLIMEVR_DEV (example)
const int MCU_TYPE_ID   = 2;  // ESP32
const int DATA_SUPPORT_ID = 1; // FLEX_RESISTANCE
// Smoothing base factors (adaptive EMA will vary between these)
const float ALPHA_MIN = 0.06f;   // steady hand → smoother
const float ALPHA_MAX = 0.40f;   // fast motion → more responsive

// Left/Right hand bone positions
const int BONE_POSITIONS_LEFT[]  = {22, 25, 28, 31, 34};
const int BONE_POSITIONS_RIGHT[] = {37, 40, 43, 46, 49};
// Choose which to use:
const int* BONE_POSITIONS = BONE_POSITIONS_LEFT;

// ========== FILTER SETTINGS ==========
const int   MED_WIN = 5;             // median window (odd, small is fine)
const float HAMPEL_K = 3.0f;         // outlier threshold in MAD units
const float EPS_MV = 5.0f;           // minimum mV to avoid div-by-zero
// Scale for adaptive alpha: small deltas → ALPHA_MIN, big deltas → ALPHA_MAX
const float DELTA_LO = 150.0f;       // ohms (below this, very smooth)
const float DELTA_HI = 2500.0f;      // ohms (above this, very responsive)

// Optional slope limit (ohms per update). Set >0 to enable; 0 disables.
const float MAX_DELTA_PER_UPDATE = 0.0f; // e.g., 120.0f for rate limiting

// ========== STATE ==========
float currentResistance[NUM_SENSORS] = {0};

// Per-sensor circular buffers for median/MAD
float ringBuf[NUM_SENSORS][MED_WIN];
uint8_t ringIdx[NUM_SENSORS]  = {0};
uint8_t ringCount[NUM_SENSORS]= {0};

// ---------- HELPERS ----------
static inline float customLerp(float a, float b, float t) { return a + t * (b - a); }

static inline float safeDivide(float num, float den, float fallback) {
  return (den == 0.0f) ? fallback : (num / den);
}

// Insert a sample into the ring buffer for sensor s
static inline void rbPush(uint8_t s, float x) {
  ringBuf[s][ringIdx[s]] = x;
  ringIdx[s] = (ringIdx[s] + 1) % MED_WIN;
  if (ringCount[s] < MED_WIN) ringCount[s]++;
}

// Compute median of current window for sensor s
float rbMedian(uint8_t s) {
  uint8_t n = ringCount[s];
  float tmp[MED_WIN];
  for (uint8_t i = 0; i < n; i++) tmp[i] = ringBuf[s][i];
  // insertion sort (n <= 5 → trivial)
  for (uint8_t i = 1; i < n; i++) {
    float key = tmp[i];
    int j = i - 1;
    while (j >= 0 && tmp[j] > key) { tmp[j+1] = tmp[j]; j--; }
    tmp[j+1] = key;
  }
  if (n == 0) return 0.0f;
  if (n & 1) return tmp[n/2];
  return 0.5f * (tmp[n/2 - 1] + tmp[n/2]);
}

// Compute MAD (median absolute deviation) for sensor s around provided median
float rbMAD(uint8_t s, float med) {
  uint8_t n = ringCount[s];
  float dev[MED_WIN];
  for (uint8_t i = 0; i < n; i++) dev[i] = fabsf(ringBuf[s][i] - med);
  // insertion sort
  for (uint8_t i = 1; i < n; i++) {
    float key = dev[i];
    int j = i - 1;
    while (j >= 0 && dev[j] > key) { dev[j+1] = dev[j]; j--; }
    dev[j+1] = key;
  }
  if (n == 0) return 0.0f;
  if (n & 1) return dev[n/2];
  return 0.5f * (dev[n/2 - 1] + dev[n/2]);
}

// Map |delta| to [ALPHA_MIN, ALPHA_MAX]
float adaptiveAlpha(float deltaAbs) {
  float t = 0.0f;
  if (deltaAbs <= DELTA_LO) t = 0.0f;
  else if (deltaAbs >= DELTA_HI) t = 1.0f;
  else t = (deltaAbs - DELTA_LO) / (DELTA_HI - DELTA_LO);
  return ALPHA_MIN + t * (ALPHA_MAX - ALPHA_MIN);
}

// One robust smoothing step for sensor s
float smoothFlex(uint8_t s, float rawOhms) {
  // 1) Update window
  rbPush(s, rawOhms);

  // 2) Median baseline
  float med = rbMedian(s);

  // 3) Hampel outlier clamp using MAD
  float mad = rbMAD(s, med);
  // For small windows MAD can be tiny → minimum floor
  if (mad < 1.0f) mad = 1.0f;
  float diff = rawOhms - med;
  float maxDev = HAMPEL_K * mad;
  if (diff > maxDev) rawOhms = med + maxDev;
  else if (diff < -maxDev) rawOhms = med - maxDev;

  // 4) Adaptive EMA (EMA target = median-clamped value)
  float delta = rawOhms - currentResistance[s];
  float a = adaptiveAlpha(fabsf(delta));
  float ema = currentResistance[s] + a * delta;

  // 5) Optional slope limit per update (anti-jerk)
  if (MAX_DELTA_PER_UPDATE > 0.0f) {
    float d = ema - currentResistance[s];
    if (d >  MAX_DELTA_PER_UPDATE) ema = currentResistance[s] + MAX_DELTA_PER_UPDATE;
    if (d < -MAX_DELTA_PER_UPDATE) ema = currentResistance[s] - MAX_DELTA_PER_UPDATE;
  }

  return ema;
}

// ---------- PACKET SENDERS ----------
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

  // Wi-Fi connect
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

  // Seed buffers & filters with first stabilized readings
  for (int i = 0; i < NUM_SENSORS; i++) {
    // Fill ring with same initial sample for a clean start
    int mv = analogReadMilliVolts(FLEX_PINS[i]);
    if (mv < EPS_MV) mv = (int)EPS_MV;
    float v = mv / 1000.0f;
    float rflex = R_DIV * (safeDivide(VCC, v, 1.0f) - 1.0f);
    if (!isfinite(rflex) || rflex < 0) rflex = 0;
    if (INVERT_FLEX) rflex = -rflex;

    for (int k = 0; k < MED_WIN; k++) rbPush(i, rflex);
    currentResistance[i] = rflex;

    sendSetupSensor(i);
    Serial.printf("Init sensor %d -> %.1f (ohms%s)\n",
                  i, currentResistance[i], INVERT_FLEX ? ", inverted" : "");
  }
}


void loop() {
  for (int i = 0; i < NUM_SENSORS; i++) {
    // Calibrated ADC read
    int mv = analogReadMilliVolts(FLEX_PINS[i]);
    if (mv < EPS_MV) mv = (int)EPS_MV; // protect against 0 mV

    float v = mv / 1000.0f; // volts
    float rflex = R_DIV * (safeDivide(VCC, v, 1.0f) - 1.0f);

    // Sanity bounds
    if (!isfinite(rflex) || rflex < 0) rflex = 0;
    if (rflex > 1e6f) rflex = 1e6f;

    // Reverse (invert) before smoothing and sending
    if (INVERT_FLEX) rflex = -rflex;

    // Robust smoothing pipeline
    float smoothed = smoothFlex(i, rflex);
    currentResistance[i] = smoothed;

    // Transmit
    sendFlexResistance(i, currentResistance[i]);
  }

  delay(10); // ~100 Hz update rate
}
