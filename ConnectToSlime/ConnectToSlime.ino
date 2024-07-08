#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_wifi.h>
#include "ByteBuffer.h"

// Define the pins for the flex sensors
const int FLEX_PINS[] = {A7, A2, A3, A4, A9};
const int NUM_SENSORS = 5;
const float VCC = 3.3;
const float R_DIV = 10000.0;

// WiFi credentials
const char* ssid = "you_ssid";
const char* password = "your_password";

IPAddress slimevrIp;
const int slimevrPort = 6969;
long packetId = 1;

WiFiUDP udp;
ByteBuffer packetBuffer;

// Array to store tracker IDs assigned by the server
int trackerIDs[NUM_SENSORS];

void setup() {
  Serial.begin(9600);

  // Initialize the flex sensor pins
  for (int i = 0; i < NUM_SENSORS; i++) {
    pinMode(FLEX_PINS[i], INPUT);
  }

  // Connect to WiFi
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
  slimevrIp = IPAddress(255, 255, 255, 255);

  packetBuffer.init(128);

  // Send handshake to the server
  sendHandshake();

  // Wait for server response and tracker ID assignment
  for (int i = 0; i < NUM_SENSORS; i++) {
    int id = requestTrackerID();
    if (id != -1) {
      trackerIDs[i] = id;
    } else {
      Serial.println("Failed to get tracker ID for sensor " + String(i));
    }
  }
}

void loop() {
  // Read flex sensor data and send it to the server
  for (int i = 0; i < NUM_SENSORS; i++) {
    int flexADC = analogRead(FLEX_PINS[i]);
    float flexV = flexADC * VCC / 1023.0; // Convert ADC value to voltage
    float flexR = R_DIV * (VCC / flexV - 1.0); // Calculate flex sensor resistance
    sendFlexResistance(trackerIDs[i], flexR); // Send resistance data with assigned tracker ID
  }
  delay(60); // Wait 60ms before next read
}

void sendHandshake() {
  packetBuffer.clear();
  packetBuffer.putInt(3);                                // Packet header for handshake
  packetBuffer.putLong(packetId++);                      // Packet ID
  packetBuffer.putInt(2);                                // Board type (Example: 2 for custom board)
  packetBuffer.putInt(1);                                // IMU type (Example: 1 for generic IMU)
  packetBuffer.putInt(1);                                // MCU type (Example: 1 for ESP32)
  for (int i = 0; i < 3; i++) packetBuffer.putInt(0);    // Unused IMU info
  packetBuffer.putInt(1);                                // Firmware build number
  String fwString = "SlimeVRConnect";
  packetBuffer.put(fwString.length());                   // Length of firmware string
  for (int i = 0; i < fwString.length(); i++) packetBuffer.put(fwString[i]); // Firmware string
  byte mac[6];
  WiFi.macAddress(mac);
  for (int i = 0; i < 6; i++) packetBuffer.put(mac[i]);  // MAC address

  udp.beginPacket(slimevrIp, slimevrPort);
  udp.write(packetBuffer.array(), packetBuffer.size());
  udp.endPacket();
}

int requestTrackerID() {
  sendSetupSensor();

  unsigned long startTime = millis();
  while (millis() - startTime < 5000) {  // Wait for 5 seconds to get the tracker ID
    int packetSize = udp.parsePacket();
    if (packetSize) {
      char incomingPacket[255];
      udp.read(incomingPacket, 255);
      return (int)incomingPacket[1];  // Assume the server sends the ID in the second byte
    }
    delay(100);
  }
  return -1;  // Return -1 if no ID is received within 5 seconds
}

void sendSetupSensor() {
  packetBuffer.clear();
  packetBuffer.putInt(15);                               // Packet header for setup sensor
  packetBuffer.putLong(packetId++);                      // Packet ID
  packetBuffer.put((byte)0);                             // Tracker ID placeholder, server will assign
  packetBuffer.put((byte)0);                             // Sensor status
  packetBuffer.put((byte)1);                             // IMU type (Example: 1 for generic IMU)

  udp.beginPacket(slimevrIp, slimevrPort);
  udp.write(packetBuffer.array(), packetBuffer.size());
  udp.endPacket();
}

void sendFlexResistance(int sensorId, float resistance) {
  packetBuffer.clear();
  packetBuffer.putInt(24);                               // Packet header for flex resistance
  packetBuffer.putLong(packetId++);                      // Packet ID
  packetBuffer.put((byte)sensorId);                      // Sensor ID
  packetBuffer.putFloat(resistance);                     // Flex resistance value

  udp.beginPacket(slimevrIp, slimevrPort);
  udp.write(packetBuffer.array(), packetBuffer.size());
  udp.endPacket();
}
