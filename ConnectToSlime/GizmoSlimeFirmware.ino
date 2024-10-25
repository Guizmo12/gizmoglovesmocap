#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_wifi.h>
#include "ByteBuffer.h"

// Define the pins for the flex sensors and their corresponding bone positions
const int FLEX_PINS[] = {A9, A7, A2, A3, A4};
// Use LEFT_HAND bones
//const int BONE_POSITIONS_LEFT[] = {22, 25, 28, 31, 34}; // LEFT_THUMB_INTERMEDIATE, LEFT_INDEX_INTERMEDIATE, LEFT_MIDDLE_INTERMEDIATE, LEFT_RING_INTERMEDIATE, LEFT_LITTLE_INTERMEDIATE
// Uncomment the following line for RIGHT_HAND bones
const int BONE_POSITIONS_RIGHT[] = {37, 40, 43, 46, 49}; // RIGHT_THUMB_INTERMEDIATE, RIGHT_INDEX_INTERMEDIATE, RIGHT_MIDDLE_INTERMEDIATE, RIGHT_RING_INTERMEDIATE, RIGHT_LITTLE_INTERMEDIATE

// Choose which set of bone positions to use
const int* BONE_POSITIONS = BONE_POSITIONS_RIGHT; // Change to BONE_POSITIONS_RIGHT for right hand

const int NUM_SENSORS = 5;
const float VCC = 3.3; // Voltage supply
const float R_DIV = 10000.0; // Resistance divider

// WiFi credentials
const char* ssid = "your_ssid";
const char* password = "your_password";

IPAddress slimevrIp(255, 255, 255, 255); // Broadcast address for initial setup
const int slimevrPort = 6969;
const int protocolVersion = 18; // First version with flex and trackerposition support
long packetId = 1;

WiFiUDP udp;
ByteBuffer packetBuffer;

// Enum values as per server's definition
const int IMU_TYPE_ID = 1; // Example: MPU9250
const int BOARD_TYPE_ID = 2; // Example: SLIMEVR_DEV
const int MCU_TYPE_ID = 2; // Example: ESP32
const int DATA_SUPPORT_ID = 1; // FLEX_RESISTANCE
const float SMOOTH_FACTOR = 0.1f; // Smoothing factor
float currentResistance[NUM_SENSORS] = {0};

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
    packetBuffer.init(128);
    sendHandshake();

    // Initialize additional sensors (starting at ID 1)
    for (int i = 0; i < NUM_SENSORS; i++) {
        sendSetupSensor(i);
        Serial.print("Tracker ID for sensor ");
        Serial.print(i);
        Serial.print(" is ");
        Serial.println(i);
    }
}

void loop() {
    for (int i = 0; i < NUM_SENSORS; i++) {
        int flexADC = analogRead(FLEX_PINS[i]);
        float flexV = flexADC * VCC / 1023.0; // Convert ADC value to voltage
        float flexR = R_DIV * (VCC / flexV - 1.0); // Calculate flex sensor resistance
        flexR = -flexR;

        // Apply smoothing
        currentResistance[i] = customLerp(currentResistance[i], flexR, SMOOTH_FACTOR);

        sendFlexResistance(i, currentResistance[i]); // Send smoothed resistance data with assigned tracker ID
    }
    delay(10); // Wait 10ms before next read
}

// Linear interpolation function
float customLerp(float a, float b, float t) {
    return a + t * (b - a);
}

void sendHandshake() {
    packetBuffer.clear();
    packetBuffer.putInt(3); // Packet header for handshake
    packetBuffer.putLong(packetId++); // Packet ID
    packetBuffer.putInt(BOARD_TYPE_ID); // Board type
    packetBuffer.putInt(0); // IMU type (unused)
    packetBuffer.putInt(MCU_TYPE_ID); // MCU type
    for (int i = 0; i < 3; i++) packetBuffer.putInt(0); // IMU info (unused)
    packetBuffer.putInt(protocolVersion); // Protocol version
    String fwString = "GizmoGloves";
    packetBuffer.put(fwString.length()); // Length of firmware string
    for (int i = 0; i < fwString.length(); i++) packetBuffer.put(fwString[i]); // Firmware string
    byte mac[6];
    WiFi.macAddress(mac);
    for (int i = 0; i < 6; i++) packetBuffer.put(mac[i]); // MAC address

    udp.beginPacket(slimevrIp, slimevrPort);
    udp.write(packetBuffer.array(), packetBuffer.size());
    udp.endPacket();
}

void sendSetupSensor(int trackerId) {
    packetBuffer.clear();
    packetBuffer.putInt(15); // Packet header for setup sensor
    packetBuffer.putLong(packetId++); // Packet ID
    packetBuffer.put((byte)trackerId); // Tracker ID assigned by firmware
    packetBuffer.put((byte)0); // Sensor status
    packetBuffer.put((byte)IMU_TYPE_ID); // IMU type
    
    // Add tracker position and data type (resistance flex data)
    packetBuffer.put((byte)BONE_POSITIONS[trackerId]); // TrackerPosition
    packetBuffer.put((byte)DATA_SUPPORT_ID); // This tracker should expect resistance flex data

    udp.beginPacket(slimevrIp, slimevrPort);
    udp.write(packetBuffer.array(), packetBuffer.size());
    udp.endPacket();
}

void sendFlexResistance(int sensorId, float resistance) {
    packetBuffer.clear();
    packetBuffer.putInt(24); // Packet header for flex resistance
    packetBuffer.putLong(packetId++); // Packet ID
    packetBuffer.put((byte)sensorId); // Sensor ID assigned by firmware
    packetBuffer.putFloat(resistance); // Flex resistance value

    udp.beginPacket(slimevrIp, slimevrPort);
    udp.write(packetBuffer.array(), packetBuffer.size());
    udp.endPacket();
}
