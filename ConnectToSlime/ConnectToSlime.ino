#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_wifi.h>
#include "ByteBuffer.h"

// Define the pins for the flex sensors and their corresponding bone positions
const int FLEX_PINS[] = {A9, A7, A2, A3, A4};
// Uncomment the following line for LEFT_HAND bones
//const int BONE_POSITIONS[] = {22, 25, 28, 31, 34}; // LEFT_THUMB_INTERMEDIATE, LEFT_INDEX_INTERMEDIATE, LEFT_MIDDLE_INTERMEDIATE, LEFT_RING_INTERMEDIATE, LEFT_LITTLE_INTERMEDIATE
// Uncomment the following line for RIGHT_HAND bones
const int BONE_POSITIONS[] = {37, 40, 43, 46, 49}; // RIGHT_THUMB_INTERMEDIATE, RIGHT_INDEX_INTERMEDIATE, RIGHT_MIDDLE_INTERMEDIATE, RIGHT_RING_INTERMEDIATE, RIGHT_LITTLE_INTERMEDIATE

const int NUM_SENSORS = 5;
const float VCC = 3.3; // Voltage supply
const float R_DIV = 10000.0; // Resistance divider

// WiFi credentials
const char* ssid = "your_ssid";
const char* password = "your_password";

IPAddress slimevrIp(255, 255, 255, 255); // Broadcast address for initial setup
const int slimevrPort = 6969;
long packetId = 1;

WiFiUDP udp;
ByteBuffer packetBuffer;

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

    // Send handshake to the server (note: this creates a sensor)
    sendHandshake();

    // Initialize additional sensors (starting at ID 1)
    for (int i = 1; i < NUM_SENSORS; i++) {
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
        sendFlexResistance(i, flexR); // Send resistance data with assigned tracker ID
    }
    delay(60); // Wait 60ms before next read
}

void sendHandshake() {
    packetBuffer.clear();
    packetBuffer.putInt(3); // Packet header for handshake
    packetBuffer.putLong(packetId++); // Packet ID
    packetBuffer.putInt(2); // Board type (Example: 2 for custom board)
    packetBuffer.putInt(1); // IMU type (Example: 1 for generic IMU)
    packetBuffer.putInt(1); // MCU type (Example: 1 for ESP32)
    for (int i = 0; i < 3; i++) packetBuffer.putInt(0); // Unused IMU info
    packetBuffer.putInt(1); // Firmware build number
    String fwString = "SlimeVRConnect";
    packetBuffer.put(fwString.length()); // Length of firmware string
    for (int i = 0; i < fwString.length(); i++) packetBuffer.put(fwString[i]); // Firmware string
    byte mac[6];
    WiFi.macAddress(mac);
    for (int i = 0; i < 6; i++) packetBuffer.put(mac[i]); // MAC address
    
    // Add tracker position and data type (resistance flex data)
    packetBuffer.putInt(BONE_POSITIONS[0]); // TrackerPosition for handshake
    packetBuffer.putInt(1); // This tracker should expect resistance flex data

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
    packetBuffer.put((byte)1); // IMU type (Example: 1 for generic IMU)
    
    // Add tracker position and data type (resistance flex data)
    packetBuffer.putInt(BONE_POSITIONS[trackerId]); // TrackerPosition
    packetBuffer.putInt(1); // This tracker should expect resistance flex data

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
