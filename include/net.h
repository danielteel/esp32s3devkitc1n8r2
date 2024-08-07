#pragma once
#include <WiFi.h>

class Net {
    public:
        Net(String address, uint16_t port);

        void loop();

        void subscribe(String name);
        void unsubscribe(String name);

        void publish(String name, String payload);
        void unpublish(String name);

        // void require(String name);
        // void unrequire(String name);
        // void update(String name, String payload);

        // void listen(String name);
        // void unlisten(String name);
        // void message(String name, String payload);

    private:
        WiFiClient Client;
        String hostAddress;
        uint16_t port;

        uint32_t clientsHandshake;
        uint32_t serversHandshake;



        void (*published)(String name, String payload)=nullptr;
        //void (*messaged)(String name, String payload)=nullptr;


    private:
        const uint32_t connectAttemptInterval=2000;
        uint32_t lastConnectAttempt=0;
        void attemptToConnect();
};