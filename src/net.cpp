#include <Arduino.h>
#include <WiFi.h>
#include <Esp.h>
#include "encro.h"
#include "net.h"
#include "utils.h"

const uint8_t packetMagicBytes[]={73, 31};
const uint8_t handshakeMagicBytes[]={13, 37};

Net::Net(String deviceName, String encroKey, String address, uint16_t port){
    this->deviceName=deviceName;
    this->hostAddress=address;
    this->port=port;

    buildKey(encroKey.c_str(), this->encroKey);
}

void Net::attemptToConnect(){
    if (!this->Client.connected()){
        
        this->netStatus=NETSTATUS::NOTHING;
        this->recvState=RECVSTATE::LEN1;
        if (isTimeToExecute(this->lastConnectAttempt, connectAttemptInterval)){
            if (this->Client.connect(this->hostAddress.c_str(), this->port)){
                this->clientsHandshake=esp_random() ^ esp_random();

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
    switch (netStatus){
        case NETSTATUS::INITIAL_SENT:
            serverHandshakeNumber=recvdHandshake+1;
            break;
    }
}

void Net::byteReceived(uint8_t data){
    switch (recvState){
        case RECVSTATE::LEN1:
            packetLength=data;
            packetState=PACKETWRITESTATE::LEN2;
            break;
        case RECVSTATE::LEN2:
            packetLength|=(data<<8);
            packetState=PACKETWRITESTATE::LEN3;
            break;
        case RECVSTATE::LEN3:
            packetLength|=(data<<16);
            packetState=PACKETWRITESTATE::LEN4;
            break;
        case RECVSTATE::LEN4:
            packetLength|=(data<<24);
            payloadRecvdCount=0;
            if (packetPayload){
                free(packetPayload);
                packetPayload=nullptr;
            }
            if (packetLength==0){
                packetState=PACKETWRITESTATE::LEN1;
            }else{
                packetState=PACKETWRITESTATE::PAYLOAD;
                packetPayload=(uint8_t*)malloc(packetLength);
            }
            break;
        case RECVSTATE::PAYLOAD:
            packetPayload[payloadRecvdCount]=data;
            payloadRecvdCount++;
            if (payloadRecvdCount>=packetLength){
                packetState=PACKETWRITESTATE::LEN1;
                uint32_t recvdHandshake=0;
                uint32_t decryptedLength=0;
                bool errorOccurred=false;
                uint8_t* plainText=decrypt(recvdHandshake, packetPayload, packetLength, decryptedLength, encroKey, errorOccurred);
                free(packetPayload);
                //Full packet recieved, send it off for processing
                packetRecieved(recvdHandshake, plainText, decryptedLength);
                delete[] plainText;
                plainText=nullptr;
                packetPayload=nullptr;
            }
            break;
    }
}

void Net::loop(){
    this->attemptToConnect();
    if (this->Client.connected()){
        int bytesAvailable = this->Client.available();
        if (bytesAvailable){
            this->byteReceived(this->Client.read());
        }
    }
}

WiFiClient Client;

uint32_t handshakeNumber=0;
uint32_t serverHandshakeNumber=0;



const char* deviceName = "Solar";
const char* encroKeyString = "";
const char* handshakeMessage="Lights Off:void:0,Lights On:void:1,Auto:void:2";
uint8_t encryptionKey[32];

void buildKey(const char* keyString, uint8_t* key){
    char tempHex[] = { 0,0,0 };
    for (uint8_t i = 0; i < 64; i += 2) {
        tempHex[0] = keyString[i];
        tempHex[1] = keyString[i + 1];
        key[i >> 1] = strtoul(tempHex, nullptr, 16);
    }
}
void onPacket(uint8_t* data, uint32_t dataLength){
    //process data here
    //e.g. if (data[0]==0xF) { digitalWrite(14, HIGH); }
}

void sendInitialHandshake(){
    uint32_t encryptedLength;
    uint8_t* encrypted=encrypt(handshakeNumber, (uint8_t*)handshakeMessage, strlen(handshakeMessage), encryptedLength, encroKey);
    if (encrypted){
        Client.write(handshakeMagicBytes, 2);
        Client.write((uint8_t) strlen(deviceName));
        Client.write(deviceName);
        Client.write((uint8_t*)&encryptedLength, 4);
        Client.write(encrypted, encryptedLength);
        delete[] encrypted;
    }else{
        Serial.println("failed to encrypt in sendInitialHandshake()");
    }
}

void sendPacket(const void* data, uint32_t dataLength){
    uint32_t encryptedLength;
    uint8_t* encrypted=encrypt(handshakeNumber, (const uint8_t*)data, dataLength, encryptedLength, encroKey);
    if (encrypted){
        Client.write(packetMagicBytes, 2);
        Client.write((uint8_t*)&encryptedLength, 4);
        Client.write(encrypted, encryptedLength);
        Serial.print("Sent message with handshake ");
        Serial.println(handshakeNumber);
        delete[] encrypted;
        handshakeNumber++;
    }else{
        Serial.println("failed to encrypt in sendPacket()");
    }
}

typedef enum {
    MAGIC1,
    MAGIC2,
    LEN1,
    LEN2,
    LEN3,
    LEN4,
    PAYLOAD
} PACKETWRITESTATE;

PACKETWRITESTATE packetState=PACKETWRITESTATE::MAGIC1;

uint8_t packetType=0;
uint32_t packetLength=0;
uint8_t* packetPayload = nullptr;
uint32_t packetPayloadWriteIndex = 0;
bool haveRecievedServerHandshakeNumber=false;


void resetPacketStatus(){
    if (packetPayload){
        delete[] packetPayload;
        packetPayload=nullptr;
    }
    packetState=PACKETWRITESTATE::MAGIC1;
    packetLength=0;
    packetPayloadWriteIndex=0;
    haveRecievedServerHandshakeNumber=false;
    serverHandshakeNumber=0;
}

void onError(const char* errorMsg){
    if (errorMsg){
        Serial.print("Error: ");
        Serial.println(errorMsg);
    }else{
        Serial.println("Error occured");
    }
    Client.stop();
}

void dataRecieved(uint8_t byte){
    switch (packetState){
        case PACKETWRITESTATE::MAGIC1:
            if (!haveRecievedServerHandshakeNumber){
                if (byte!=handshakeMagicBytes[0]){
                    onError("magic1 initial byte is incorrect");
                    return;
                }
            }else{
                if (byte!=packetMagicBytes[0]){
                    onError("magic1 byte is incorrect");
                    Serial.print(byte);
                    return;
                } 
            }

            packetState=PACKETWRITESTATE::MAGIC2;
            break;
        case PACKETWRITESTATE::MAGIC2:
            if (!haveRecievedServerHandshakeNumber){
                if (byte!=handshakeMagicBytes[1]){
                    onError("magic2 initial byte is incorrect");
                    return;
                }
            }else{
                if (byte!=packetMagicBytes[1]){
                    onError("magic2 byte is incorrect");
                    return;
                } 
            }
            packetState=PACKETWRITESTATE::LEN1;
            break;
        case PACKETWRITESTATE::LEN1:
            memmove(((uint8_t*)&packetLength)+0, &byte, 1);
            packetState=PACKETWRITESTATE::LEN2;
            break;
        case PACKETWRITESTATE::LEN2:
            memmove(((uint8_t*)&packetLength)+1, &byte, 1);
            packetState=PACKETWRITESTATE::LEN3;
            break;
        case PACKETWRITESTATE::LEN3:
            memmove(((uint8_t*)&packetLength)+2, &byte, 1);
            packetState=PACKETWRITESTATE::LEN4;
            break;
        case PACKETWRITESTATE::LEN4:
            memmove(((uint8_t*)&packetLength)+3, &byte, 1);
            if (packetPayload){
                delete[] packetPayload;
                packetPayload=nullptr;
            }
            if (packetLength>0x0FFFFF){
                onError("packet length > 0x0FFFFF");
                return;
            }
            Serial.printf("Recvd Len: %u\n", packetLength);
            packetPayload=new uint8_t[packetLength];//need to clean this up on an error
            packetState=PACKETWRITESTATE::PAYLOAD;
            packetPayloadWriteIndex=0;
            break;
        case PACKETWRITESTATE::PAYLOAD:
            packetPayload[packetPayloadWriteIndex]=byte;
            packetPayloadWriteIndex++;
            if (packetPayloadWriteIndex>=packetLength){
                uint32_t decryptedLength;
                uint32_t recvdServerHandshakeNumber;
                bool errorOccured = false;
                uint8_t* decrypted = decrypt(recvdServerHandshakeNumber, packetPayload, packetLength, decryptedLength, encroKey, errorOccured);
                delete[] packetPayload;
                packetPayload=nullptr;
                if (errorOccured){
                    onError("failed to decrypt");
                }else{       
                    if (!haveRecievedServerHandshakeNumber){
                        serverHandshakeNumber=recvdServerHandshakeNumber;
                        haveRecievedServerHandshakeNumber=true;
                    }else{
                        //Send off decrypted packet for processing
                        if (recvdServerHandshakeNumber==serverHandshakeNumber){
                            serverHandshakeNumber++;
                            onPacket(decrypted, decryptedLength);
                        }else{
                            onError("incorrect handshake number recieved");
                            Serial.printf("Recvd: %u  Expected: %u\n", recvdServerHandshakeNumber, serverHandshakeNumber);
                            if (decrypted){
                                delete[] decrypted;
                                decrypted=nullptr;
                            }
                            return;
                        }
                    }
                    if (decrypted){
                        delete[] decrypted;
                        decrypted=nullptr;
                    }
                }
                packetState=PACKETWRITESTATE::MAGIC1;
            }
            break;
    }
}