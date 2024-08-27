#include <Arduino.h>
#include <WiFi.h>
#include <Esp.h>
#include "utils.h"
#include "secrets.h"
#include "net.h"


const char *WiFiSSID = SECRET_WIFI_SSID;
const char *WiFiPass = SECRET_WIFI_PASS;

Net NetClient("device1", SECRET_ENCROKEY, "192.168.50.178", 4004);

void setup(){
    Serial.begin(115200);
    Serial.println("Initializing...");

    WiFi.mode(WIFI_STA);
    WiFi.begin(WiFiSSID, WiFiPass);
    WiFi.setSleep(WIFI_PS_NONE);

    //neopixelWrite(RGB_BUILTIN,0,0,0);
}

void loop(){
    static uint32_t lastPrintTime = 0;
    static uint32_t lastSendTime=0;
    static uint32_t sendCount=0;

    if (isTimeToExecute(lastPrintTime, 10000)){
        Serial.print("Free heap:");
        Serial.println(esp_get_free_heap_size());
    }

    if (WiFi.status() != WL_CONNECTED){//Reconnect to WiFi
        if (WiFi.status() == WL_CONNECT_FAILED || WiFi.status()==WL_CONNECTION_LOST || WiFi.status()==WL_DISCONNECTED || WiFi.status()==WL_IDLE_STATUS){
            WiFi.disconnect();
            WiFi.begin(WiFiSSID, WiFiPass);
            Serial.print("WiFi connection failed");
        }
        Serial.println("Trying to connect to WiFi..."+String(millis()));
        Serial.println(WiFi.status());
        delay(15000);
    }else{

        NetClient.loop();
        if (NetClient.connected() && isTimeToExecute(lastSendTime, 75)){
            String msg = "count: ";
            msg+=sendCount;
            NetClient.sendString(msg);
            sendCount++;
        }
    }

}