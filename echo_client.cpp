#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <chrono>
#include <random>
#include "protocol.hpp"

const int RETRANS_TIMEOUT_MS = 500;

int main(){
    // Client socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfd < 0){
        perror("Socket creation failed");
        return 1;
    }

    // client socket address struct
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080);
    if(inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr) <= 0){
        perror("presentation to network failed");
        return 1;
    }

    // random number
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, 100);

    // window slot struct
    struct WindowSlot
    {   
        Packet pkt;
        bool isAcked;
        std::chrono::steady_clock::time_point sendTime;
    };

    // Ring buffer state
    const int WINDOW_SIZE = 5;
    WindowSlot window[WINDOW_SIZE];

    // Sliding window pointer
    uint32_t baseSeq = 1;      // older unacknowledged sequence number
    uint32_t nextSeq = 1;      // the sequence number of next packet to send

    while(true)
    {
        if(nextSeq < baseSeq + WINDOW_SIZE)
        {
            int index = nextSeq % WINDOW_SIZE;

            window[index].pkt.header.seqNum = htonl(nextSeq);
            std::memset(window[index].pkt.payLoad, 0, sizeof(window[index].pkt.payLoad));
            strcpy(window[index].pkt.payLoad, "Hello!");
            window[index].isAcked = false;
            window[index].sendTime = std::chrono::steady_clock::now();
            window[index].pkt.checkSum = htons(calculateCheckSum(window[index].pkt.payLoad, sizeof(window[index].pkt.payLoad)));

            // Intentional Fault injection
            Packet outPacket = window[index].pkt;
            if(dist(rng) < 20)
            {
                outPacket.checkSum += 1;
                std::cout << "[Fault Injection] Corrupted checksum for sequence : " << nextSeq << std::endl;
            }

            if(sendto(sockfd, &outPacket, sizeof(outPacket), 0, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0)
            {
                perror("send to failed");
                return 1;
            }

            nextSeq++;
        }

        Packet ackRecvdPkt{};
        socklen_t serverAddrLen = sizeof(serverAddr);
        ssize_t bytesRecived = recvfrom(sockfd, &ackRecvdPkt, sizeof(ackRecvdPkt), MSG_DONTWAIT, (sockaddr*)&serverAddr, &serverAddrLen);
        uint16_t expectedCheckSumAckPkt = calculateCheckSum(ackRecvdPkt.payLoad, sizeof(ackRecvdPkt.payLoad));
        if((bytesRecived > 0 )&& (ackRecvdPkt.header.isAck) && (expectedCheckSumAckPkt == ntohs(ackRecvdPkt.checkSum)))
        {
            uint32_t ackSeq = ntohl(ackRecvdPkt.header.seqNum);
            int windowIndex = ackSeq % WINDOW_SIZE;
            if(ntohl(window[windowIndex].pkt.header.seqNum) == ackSeq)
            {
                window[ackSeq % WINDOW_SIZE].isAcked = true;
            }
        }
        
        while(window[baseSeq % WINDOW_SIZE].isAcked && baseSeq < nextSeq){
            baseSeq++;
        }

        // Timer sweep
        for(uint32_t seq = baseSeq; seq < nextSeq; seq++){
            int index = seq % WINDOW_SIZE;
            if(!window[index].isAcked){
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - window[index].sendTime).count();
                if(elapsed > RETRANS_TIMEOUT_MS){
                    if(sendto(sockfd, &window[index].pkt, sizeof(window[index].pkt), 0, (sockaddr *)&serverAddr, sizeof(serverAddr)) > 0){
                        window[index].sendTime = std::chrono::steady_clock::now();
                    }
                    
                }
            }
        }
    }
    close(sockfd);
    return 0;
}