#include <Arduino.h>
#include <WiFi.h>
#include <Esp.h>
#include "utils.h"
#include "secrets.h"
#include "net.h"
//#include "DHTesp.h" 

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_BME280.h>
#include "HX711.h"

const char *WiFiSSID = SECRET_WIFI_SSID;
const char *WiFiPass = SECRET_WIFI_PASS;

Net NetClient("device1", SECRET_ENCROKEY, "192.168.50.178", 4004);


//DHTesp dhtSensor1;
Adafruit_BME280 bme; // use I2C interface

HX711 scale;

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

    
	//dhtSensor1.setup(17, DHTesp::DHT22);
    
    Wire.setPins(16, 15);
    if (!bme.begin(0x76)){
        Serial.println(F("Could not find a valid BME280 sensor, check wiring!"));
    }
    bme.setSampling(Adafruit_BME280::MODE_FORCED,
                    Adafruit_BME280::SAMPLING_X1, // temperature
                    Adafruit_BME280::SAMPLING_X1, // pressure
                    Adafruit_BME280::SAMPLING_X1, // humidity
                    Adafruit_BME280::FILTER_OFF   );
  
    scale.begin(12, 11);
    scale.tare();
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
        if (isTimeToExecute(lastPrintTime, 1000)){
            // NetClient.sendString(String("CPU: ")+String(temperatureRead()));
            // NetClient.sendString(String("\n"));
            // NetClient.sendString(String("T1: ")+String(dhtSensor1.getTemperature()));
            // NetClient.sendString(String("H1: ")+String(dhtSensor1.getHumidity()));
            bme.takeForcedMeasurement();
            NetClient.sendString(String(millis())+","+String(bme.readTemperature())+String(",")+String(bme.readHumidity())+String(",")+String(bme.readPressure())+String(",")+String(bme.readAltitude(SENSORS_PRESSURE_SEALEVELHPA))+String(",")+String(scale.get_units()));
        }
    }
}