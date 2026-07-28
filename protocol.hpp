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
    char payLoad[1024];
};