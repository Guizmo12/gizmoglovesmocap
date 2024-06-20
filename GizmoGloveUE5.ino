#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>

// WiFi Credentials
const char* ssid = "Wifi6";
const char* password = "Cather1ne1906";

// Server details
const char* serverName = "http://your_unreal_server_address/receive_data";

// Pin Configuration for Flex Sensors (using GPIO)
const int flexPins[5] = {26, 25, 34, 39, 36}; // Pinky, Ring, Middle, Index, Thumb
int flexValues[5];

// Calibration values
int openHandValues[5] = {0, 0, 0, 0, 0};
int closedHandValues[5] = {0, 0, 0, 0, 0};

// Variables for timing
unsigned long previousMillis = 0;
unsigned long samplingInterval = 60; // Default sampling interval in milliseconds

// Hand configuration
String hand = "right"; // Default hand

// Web server on port 80
WebServer server(80);

void setup() {
  Serial.begin(115200);

  // Initialize Flex Sensors
  for (int i = 0; i < 5; i++) {
    pinMode(flexPins[i], INPUT);
  }

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // Setup Web server
  server.on("/", handleRoot);
  server.on("/setHand", handleSetHand);
  server.on("/setInterval", handleSetInterval);
  server.on("/calibrateOpen", handleCalibrateOpen);
  server.on("/calibrateClosed", handleCalibrateClosed);
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= samplingInterval) {
    previousMillis = currentMillis;

    // Read Flex Sensor Values
    for (int i = 0; i < 5; i++) {
      flexValues[i] = analogRead(flexPins[i]);
    }

    // Apply Filtering and Calibration
    filterAndCalibrateData();

    // Send Data to Unreal Engine
    sendDataToUnreal();
  }
}

void filterAndCalibrateData() {
  // Apply calibration
  for (int i = 0; i < 5; i++) {
    flexValues[i] = map(flexValues[i], openHandValues[i], closedHandValues[i], 0, 100); // Map calibrated values
  }
}

void sendDataToUnreal() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    String postData = "hand=" + hand + 
                      "&pinky=" + String(flexValues[0]) + 
                      "&ring=" + String(flexValues[1]) + 
                      "&middle=" + String(flexValues[2]) + 
                      "&index=" + String(flexValues[3]) + 
                      "&thumb=" + String(flexValues[4]);

    http.begin(serverName);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    int httpResponseCode = http.POST(postData);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println(httpResponseCode);
      Serial.println(response);
    } else {
      Serial.print("Error on sending POST: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
}

// Handle root URL
void handleRoot() {
  String html = "<html><body><h1>GizmoGlove MoCap</h1>";
  html += "<form action='/setHand' method='GET'>";
  html += "Hand: <select name='hand'><option value='left'>Left</option><option value='right'>Right</option></select>";
  html += "<input type='submit' value='Confirm'></form>";
  html += "<form action='/setInterval' method='GET'>";
  html += "Sampling Interval (ms): <input type='number' name='interval' value='60'>";
  html += "<input type='submit' value='Confirm'></form>";
  html += "<h2>Calibration</h2>";
  html += "<form action='/calibrateOpen' method='GET'><input type='submit' value='Calibrate Open Hand'></form>";
  html += "<form action='/calibrateClosed' method='GET'><input type='submit' value='Calibrate Closed Hand'></form>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

// Handle setting hand
void handleSetHand() {
  if (server.hasArg("hand")) {
    hand = server.arg("hand");
    server.send(200, "text/html", "Hand set to " + hand + "<br><a href='/'>Go Back</a>");
  } else {
    server.send(200, "text/html", "Invalid Request<br><a href='/'>Go Back</a>");
  }
}

// Handle setting sampling interval
void handleSetInterval() {
  if (server.hasArg("interval")) {
    samplingInterval = server.arg("interval").toInt();
    server.send(200, "text/html", "Sampling Interval set to " + String(samplingInterval) + "ms<br><a href='/'>Go Back</a>");
  } else {
    server.send(200, "text/html", "Invalid Request<br><a href='/'>Go Back</a>");
  }
}

// Handle open hand calibration
void handleCalibrateOpen() {
  for (int i = 0; i < 5; i++) {
    openHandValues[i] = analogRead(flexPins[i]);
  }
  server.send(200, "text/html", "Open hand calibrated<br><a href='/'>Go Back</a>");
}

// Handle closed hand calibration
void handleCalibrateClosed() {
  for (int i = 0; i < 5; i++) {
    closedHandValues[i] = analogRead(flexPins[i]);
  }
  server.send(200, "text/html", "Closed hand calibrated<br><a href='/'>Go Back</a>");
}
