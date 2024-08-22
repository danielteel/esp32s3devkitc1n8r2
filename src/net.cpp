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

bool Net::sendString(String str){
    return sendPacket((uint8_t*)str.c_str(), str.length());
}

bool Net::sendBinary(uint8_t* data, uint32_t dataLength){
    return sendPacket(data, dataLength);
}

bool Net::connected(){
    return netStatus==NETSTATUS::READY;
}

bool Net::sendPacket(uint8_t* data, uint32_t dataLength){
    uint32_t encryptedLength;
    uint8_t* encrypted=encrypt(this->clientsHandshake, data, dataLength, encryptedLength, this->encroKey);
    this->clientsHandshake++;
    if (encrypted){
        this->Client.write((uint8_t*)&encryptedLength, 4);
        this->Client.write(encrypted, encryptedLength);
        this->Client.flush();
        return true;
    }
    return false;
}

void Net::packetRecieved(uint32_t recvdHandshake, uint8_t* data, uint32_t dataLength){
    if (netStatus==NETSTATUS::INITIAL_SENT){
            serversHandshake=recvdHandshake+1;
            netStatus=NETSTATUS::READY;
    }else if (netStatus==NETSTATUS::READY){
        if (recvdHandshake==serversHandshake){
            serversHandshake++;

            //Added for debugging
            if (lastData){//
                free(lastData);//
                lastData=nullptr;//
            }//
            lastData=(uint8_t*)malloc(dataLength);//
            lastDataLength=dataLength;//
            memmove(lastData, data, dataLength);//
            //End Added for Debugging

        }else{
            //throw error, wrong handshake from expected
            String errorText="Wrong handshake, expected ";
            errorText+=String(serversHandshake)+" but recvd "+String(recvdHandshake);


            //Added for Debugging
            if (lastData){//
                Serial.print("Last data was:");//
                Serial.println(String((const char*)lastData, lastDataLength));//
                free(lastData);//
                lastData=nullptr;//
            }//
            Serial.print("Current data is:");//
            Serial.println(String((const char*)data, dataLength));//
            //End Added for Debugging

            errorOccured(errorText);
            return;
        }
    }else{
            errorOccured("Unknown netStatus");
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
                    errorOccured("Error occured decrypting payload");
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
