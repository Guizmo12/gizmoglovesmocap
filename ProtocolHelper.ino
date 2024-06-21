#include "ArduinoJson.h"

String GetStatusJSONString(int wifiStatus, String localIP) 
{
    StaticJsonDocument<1024> staticJsonDocument;
    staticJsonDocument["type"] = "getStatusResult";
    staticJsonDocument["wifi_status"] = wifiStatus;
    staticJsonDocument["local_ip"] = localIP;
 
    char docBuf[1024];
    serializeJson(staticJsonDocument, docBuf);

    return String(docBuf);
}

String GetFingerDataJSONString(String sensorName, int thumb, int index, int middle, int ring, int pinky) 
{
    StaticJsonDocument<1024> staticJsonDocument;
    staticJsonDocument["type"] = "fingerData";
    staticJsonDocument["sensorName"] = sensorName;
    staticJsonDocument["Thumb"] = String(thumb);
    staticJsonDocument["Index"] = String(index);
    staticJsonDocument["Middle"] = String(middle);
    staticJsonDocument["Ring"] = String(ring);
    staticJsonDocument["Pinky"] = String(pinky);
    
    char docBuf[1024];
    serializeJson(staticJsonDocument, docBuf);

    return String(docBuf);
}
