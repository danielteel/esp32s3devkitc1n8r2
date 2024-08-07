#include <Arduino.h>
#include <WiFi.h>
#include <Esp.h>
#include "utils.h"
#include "secrets.h"


const char *WiFiSSID = SECRET_WIFI_SSID;
const char *WiFiPass = SECRET_WIFI_PASS;

void setup(){
    Serial.begin(115200);
    Serial.println("Initializing...");

    WiFi.mode(WIFI_STA);
    WiFi.begin(WiFiSSID, WiFiPass);
}

void loop(){
    static uint32_t lastPrintTime = 0;

    if (WiFi.status() != WL_CONNECTED){//Reconnect to WiFi
        if (WiFi.status() == WL_CONNECT_FAILED){
            WiFi.disconnect();
            WiFi.begin(WiFiSSID, WiFiPass);
            Serial.print("WiFi connection failed");
        }
        Serial.println("Trying to connect to WiFi...");
        delay(500);
    }else{

        if (isTimeToExecute(lastPrintTime, 1000)){
            Serial.println(millis());
        }
    }
}