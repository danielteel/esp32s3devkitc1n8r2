#pragma once
#include <WiFi.h>

enum NETSTATUS {
    NOTHING,
    INITIAL_SENT,
    INITIAL_RECVD,
    READY
};

enum RECVSTATE {
    LEN1,
    LEN2,
    LEN3,
    LEN4,
    PAYLOAD
};

class Net {
    public:
        Net(String deviceName, String encroKey, String address, uint16_t port);

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
        String deviceName;
        uint8_t encroKey[32];
        String hostAddress;
        uint16_t port;

        uint32_t clientsHandshake;
        uint32_t serversHandshake;

        NETSTATUS netStatus;
        RECVSTATE recvState;
        uint32_t packetLength;
        uint8_t* packetPayload=nullptr;
        uint32_t payloadRecvdCount=0;

        void (*published)(String name, String payload)=nullptr;
        //void (*messaged)(String name, String payload)=nullptr;


    private:
        const uint32_t connectAttemptInterval=2000;
        uint32_t lastConnectAttempt=0;
        void attemptToConnect();
        bool sendPacket(uint8_t* data, uint32_t dataLength);
        void byteReceived(uint8_t data);
        void packetRecieved(uint32_t recvdHandshake, uint8_t* data, uint32_t dataLength);


};