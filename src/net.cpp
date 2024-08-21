#include <Arduino.h>
#include <WiFi.h>
#include <Esp.h>
#include "encro.h"
#include "net.h"
#include "utils.h"


Net::Net(String deviceName, String encroKeyString, String address, uint16_t port){
    this->deviceName=deviceName;
    this->hostAddress=address;
    this->port=port;

    buildKey(encroKeyString.c_str(), encroKey);
}

Net::~Net(){
    Client.stop();

    if (packetPayload){
        free(packetPayload);
        packetPayload=nullptr;
    }
    
	for(int i = 0; i < subscribedList.size(); i++){
		String* string = subscribedList.get(i);
        delete string;
    }
    subscribedList.clear();
}

void Net::subscribe(String name){
    subscribedList.add(new String(name));
}

void Net::errorOccured(String errorText){
    Client.stop();
    
    if (packetPayload){
        free(packetPayload);
        packetPayload=nullptr;
    }

    Serial.print("Net error occurred: ");
    Serial.println(errorText);
}

void Net::attemptToConnect(){
    if (!this->Client.connected()){
        
        this->netStatus=NETSTATUS::NOTHING;
        this->recvState=RECVSTATE::LEN1;
        if (isTimeToExecute(this->lastConnectAttempt, connectAttemptInterval)){
            if (this->Client.connect(this->hostAddress.c_str(), this->port)){
                this->clientsHandshake=esp_random();

                this->Client.write((uint8_t)this->deviceName.length());
                this->Client.write(this->deviceName.c_str());
                if (sendPacket(nullptr, 0)){
                    this->netStatus=NETSTATUS::INITIAL_SENT;
                }else{
                    this->Client.stop();
                }
            }
        }
    }
}

bool Net::sendPacket(uint8_t* data, uint32_t dataLength){
    uint32_t encryptedLength;
    uint8_t* encrypted=encrypt(this->clientsHandshake, data, dataLength, encryptedLength, this->encroKey);
    this->clientsHandshake++;
    if (encrypted){
        this->Client.write(encrypted, encryptedLength);
        return true;
    }
    return false;
}

void Net::packetRecieved(uint32_t recvdHandshake, uint8_t* data, uint32_t dataLength){
    if (netStatus==NETSTATUS::INITIAL_SENT){
            serversHandshake=recvdHandshake+1;
            netStatus=NETSTATUS::INITIAL_RECVD;
    }else{
        if (recvdHandshake!=serversHandshake){
            //throw error, wrong handshake from expected
            String errorText="Wrong handshake, expected ";
            errorText+=serversHandshake+" but recvd "+recvdHandshake;
            errorOccured(errorText);
            return;
        }else{
            serversHandshake++;
        }
        
        if (netStatus==NETSTATUS::INITIAL_RECVD){
            //Send initial data
        }else if (netStatus==NETSTATUS::READY){
            //Data recieved
        }else{
            errorOccured("Unknown netStatus");
        }
    }
}

void Net::byteReceived(uint8_t data){
    switch (recvState){
        case RECVSTATE::LEN1:
            packetLength=data;
            recvState=RECVSTATE::LEN2;
            break;
        case RECVSTATE::LEN2:
            packetLength|=(data<<8);
            recvState=RECVSTATE::LEN3;
            break;
        case RECVSTATE::LEN3:
            packetLength|=(data<<16);
            recvState=RECVSTATE::LEN4;
            break;
        case RECVSTATE::LEN4:
            packetLength|=(data<<24);
            payloadRecvdCount=0;
            if (packetPayload){
                free(packetPayload);
                packetPayload=nullptr;
            }
            if (packetLength==0){
                recvState=RECVSTATE::LEN1;
            }else{
                recvState=RECVSTATE::PAYLOAD;
                packetPayload=(uint8_t*)malloc(packetLength);
            }
            break;
        case RECVSTATE::PAYLOAD:
            packetPayload[payloadRecvdCount]=data;
            payloadRecvdCount++;
            if (payloadRecvdCount>=packetLength){
                recvState=RECVSTATE::LEN1;
                uint32_t recvdHandshake=0;
                uint32_t decryptedLength=0;
                bool errorOccurred=false;
                uint8_t* plainText=decrypt(recvdHandshake, packetPayload, packetLength, decryptedLength, encroKey, errorOccurred);
                free(packetPayload);
                packetPayload=nullptr;

                if (errorOccurred){
                    Client.stop();
                    netStatus=NETSTATUS::NOTHING;
                }else{
                    packetRecieved(recvdHandshake, plainText, decryptedLength);
                }

                if (plainText) delete[] plainText;
                plainText=nullptr;
            }
            break;
    }
}

void Net::processIncoming(){
    if (this->Client.connected()){
        while (Client.available()>0){
            this->byteReceived(this->Client.read());
        }
    }
}

void Net::loop(){
    this->attemptToConnect();
    this->processIncoming();
}
