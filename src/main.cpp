#include <Arduino.h>
#include <WiFi.h>
#include <Esp.h>
#include "secrets.h"

WiFiClient Client;

const char *WiFiSSID = SECRET_WIFI_SSID;
const char *WiFiPass = SECRET_WIFI_PASS;

void setup(){
    Serial.begin(115200);
    Serial.println("Initializing...");

    WiFi.mode(WIFI_STA);
    WiFi.begin(WiFiSSID, WiFiPass);
}

bool isTimeToExecute(uint32_t &lastTime, uint32_t interval){
    uint32_t currentTime = millis();
    if ((currentTime - lastTime) >= interval || currentTime < lastTime){
        lastTime = currentTime;
        return true;
    }
    return false;
}

void loop(){
    static uint32_t lastPrintTime = 0;


    if (WiFi.status() != WL_CONNECTED){
        if (WiFi.status() == WL_CONNECT_FAILED){
            WiFi.disconnect();
            WiFi.begin(WiFiSSID, WiFiPass);
            Serial.print("WiFi connection failed");
        }
        Serial.println("Trying to connect to WiFi...");
        delay(500);
    }else{
        if (isTimeToExecute(lastPrintTime, 100)){
            Serial.println(millis());
            
            uint32_t rand = 0;//esp_random();
            neopixelWrite(RGB_BUILTIN, rand&0xFF, (rand>>8) & 0xFF, (rand>>16) & 0xFF);
        }
    }
}