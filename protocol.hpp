#pragma once
#include <cstdint>
#include <arpa/inet.h>

struct __attribute__((packed)) UDPHeader 
{
    uint32_t seqNum;
    bool isAck;
};

struct __attribute__((packed)) Packet
{
    UDPHeader header;
    uint16_t checkSum;
    char payLoad[1024];
};

inline uint16_t calculateCheckSum(const char* payload, size_t length)
{
    uint16_t checkSum = 0;
    for(size_t i = 0; i<length; i++){
        checkSum += static_cast<uint8_t>(payload[i]);
    }
    return checkSum;
}