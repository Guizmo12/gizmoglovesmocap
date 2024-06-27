#include <WiFi.h>
#include <WiFiUdp.h>

const int FLEX_PINS[] = {A1, A2, A3, A4, A0}; // Pins for A1 to A5 
const int NUM_SENSORS = 5; // Number of sensors


// Measure the voltage at 3.3V and the actual resistance of your
// 10k resistor, and enter them below:
const float VCC = 3.3; // Measured voltage of Arduino 3.3V line
const float R_DIV = 10000.0; // Measured resistance of 10k resistor

// Upload the code, then try to adjust these values to more
// accurately calculate bend degree.
const float STRAIGHT_RESISTANCE = 10300.0; // resistance when straight
const float BEND_RESISTANCE = 90000.0; // resistance at 90 deg

// Wi-Fi credentials
const char* ssid = "Wifi6";
const char* password = "Cather1ne1906";

// UDP settings
const char* udpAddress = "10.0.0.245"; // IP address of your UDP server
const int udpPort = 12345; // UDP port on the server

WiFiUDP udp;

void setup() 
{
  Serial.begin(9600);
  for (int i = 0; i < NUM_SENSORS; i++) {
    pinMode(FLEX_PINS[i], INPUT);
  }

  // Connect to Wi-Fi
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

  // Initialize UDP
  udp.begin(udpPort);
}

void loop() 
{
  String dataString = ""; // Initialize an empty string for plotting


  for (int i = 0; i < NUM_SENSORS; i++) {
    // Read the ADC, and calculate voltage and resistance from it
    int flexADC = analogRead(FLEX_PINS[i]);
    float flexV = flexADC * VCC / 1023.0;
    float flexR = R_DIV * (VCC / flexV - 1.0);

    // Append the resistance value to the data string
    dataString += String(flexR);
    if (i < NUM_SENSORS - 1) {
      dataString += "\t"; // Add a tab separator between values
    }
  }

  Serial.println(dataString); // Print the data string for plotting

  // Send data via UDP
  udp.beginPacket(udpAddress, udpPort);
  udp.print(dataString);
  udp.endPacket();

  delay(60); // Delay before next transmission
}
