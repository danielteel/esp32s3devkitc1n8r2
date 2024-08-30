#include <Arduino.h>
#include <WiFi.h>
#include <Esp.h>
#include "utils.h"
#include "secrets.h"
#include "net.h"


const char *WiFiSSID = SECRET_WIFI_SSID;
const char *WiFiPass = SECRET_WIFI_PASS;

Net NetClient("device1", SECRET_ENCROKEY, "192.168.50.178", 4004);


void packetReceived(uint8_t* data, uint32_t dataLength){
    Serial.print("NetClient recieved:");
    Serial.println(String(data, dataLength));
}

void onConnected(){
    Serial.println("NetClient Connected");
}

void onDisconnected(){
    Serial.println("NetClient disconnected");
}

void setup(){
    Serial.begin(115200);
    Serial.println("Initializing...");

    WiFi.mode(WIFI_STA);
    WiFi.begin(WiFiSSID, WiFiPass);
    WiFi.setSleep(WIFI_PS_NONE);

    NetClient.setPacketReceivedCallback(&packetReceived);
    NetClient.setOnConnected(&onConnected);
    NetClient.setOnDisconnected(&onDisconnected);
    Serial.print("PSRAM Size:");
    Serial.println(ESP.getPsramSize());
}


void loop(){
    static uint32_t lastPrintTime = 0;

    if (WiFi.status() != WL_CONNECTED){//Reconnect to WiFi
        if (WiFi.status() == WL_CONNECT_FAILED || WiFi.status()==WL_CONNECTION_LOST || WiFi.status()==WL_DISCONNECTED || WiFi.status()==WL_IDLE_STATUS){
            WiFi.disconnect();
            WiFi.begin(WiFiSSID, WiFiPass);
            Serial.print("WiFi connection failed");
        }
        Serial.println("Trying to connect to WiFi..."+String(millis()));
        Serial.println(WiFi.status());
        delay(5000);
    }
    

    if (NetClient.loop()){
        if (isTimeToExecute(lastPrintTime, 3000)){
            NetClient.sendString(String("Temp: ")+String(temperatureRead()));
        }
    }
}