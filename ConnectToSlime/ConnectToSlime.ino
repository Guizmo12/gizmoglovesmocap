#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_wifi.h>
#include "ByteBuffer.h"

const int FLEX_PINS[] = {A7, A2, A3, A4, A9};
const int FLEX_IDS[] = {1, 2, 3, 4, 5};  // What do I do here ?!
const int NUM_SENSORS = 5;
const float VCC = 3.3;
const float R_DIV = 10000.0;

const char* ssid = "your_ssid";
const char* password = "your_password";
IPAddress slimevrIp;
const int slimevrPort = 6969;
long packetId = 0;

WiFiUDP udp;
ByteBuffer packetBuffer;

void setup() {
  Serial.begin(9600);
  for (int i = 0; i < NUM_SENSORS; i++) {
    pinMode(FLEX_PINS[i], INPUT);
  }

  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  udp.begin(slimevrPort);
  slimevrIp = IPAddress(255, 255, 255, 255);  // Broadcast to find the server

  packetBuffer.init(128);
  sendHandshake();
//Slimeconnect
  String handshakeReply = "";
  while (handshakeReply != "Hey OVR =D 5") {
    int packetSize = udp.parsePacket();
    if (packetSize) {
      char incomingPacket[255];
      udp.read(incomingPacket, 255);
      handshakeReply = String(incomingPacket);
      slimevrIp = udp.remoteIP();  // Found the server, get its IP
    }
  }

  for (int i = 0; i < NUM_SENSORS; i++) {
    sendSetupSensor(i);
  }
}

void loop() {
  for (int i = 0; i < NUM_SENSORS; i++) {
    int flexADC = analogRead(FLEX_PINS[i]);
    float flexV = flexADC * VCC / 1023.0;
    float flexR = R_DIV * (VCC / flexV - 1.0);
    sendFlexResistance(FLEX_IDS[i], flexR);  // Utilise l'ID spécifique pour chaque capteur
  }
  delay(60);
}

void sendHandshake() {
  packetBuffer.clear();
  packetBuffer.putInt(3);  // const for handshake
  packetBuffer.putLong(packetId++);
  packetBuffer.putInt(0);  // boardType.id
  packetBuffer.putInt(0);  // imuType.id
  packetBuffer.putInt(0);  // mcuType.id
  packetBuffer.putInt(0);
  packetBuffer.putInt(0);
  packetBuffer.putInt(0);
  packetBuffer.putInt(17);  // firmwareBuildVersion
  const char* fwString = "GizmoFlexGlove";
  packetBuffer.put((byte)strlen(fwString));
  for (unsigned int i = 0; i < strlen(fwString); i++) {
    packetBuffer.put((byte)fwString[i]);
  }
  uint8_t macAddress[6];
  esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, macAddress);
  for (int i = 0; i < 6; i++) {
    packetBuffer.put(macAddress[i]);
  }
  udp.beginPacket(slimevrIp, slimevrPort);
  udp.write(packetBuffer.array(), packetBuffer.size());
  udp.endPacket();
}

void sendSetupSensor(int sensorId) {
  packetBuffer.clear();
  packetBuffer.putInt(15); // const for sensor info
  packetBuffer.putLong(packetId++);
  packetBuffer.put((byte)sensorId);
  packetBuffer.put(1); // sensor status, 1 is OK, 2 is ERROR
  packetBuffer.put(0); // sensor type

  udp.beginPacket(slimevrIp, slimevrPort);
  udp.write(packetBuffer.array(), packetBuffer.size());
  udp.endPacket();
}

void sendFlexResistance(int sensorId, float flexResistance) {
  packetBuffer.clear();
  packetBuffer.putInt(24);  // const for flex resistance packet
  packetBuffer.putLong(packetId++);
  packetBuffer.put((byte)sensorId);
  packetBuffer.putFloat(flexResistance);
  udp.beginPacket(slimevrIp, slimevrPort);
  udp.write(packetBuffer.array(), packetBuffer.size());
  udp.endPacket();
}
