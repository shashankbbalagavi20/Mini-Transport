#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <map>
#include "../protocol.hpp"

// ==========================================
// TARGET 1: UNIT TEST - CHECKSUM ALGORITHM
// ==========================================
TEST(ReliableUDP, ChecksumCalculation) {
    Packet pkt{};
    std::memset(pkt.payLoad, 0, sizeof(pkt.payLoad));
    strcpy(pkt.payLoad, "Test"); 
    
    uint16_t sum = calculateCheckSum(pkt.payLoad, sizeof(pkt.payLoad));
    EXPECT_EQ(sum, 416);
}

// ==========================================
// TARGET 2: INTEGRATION TEST - SLIDING WINDOW & ARQ
// ==========================================
TEST(ReliableUDP, OutOfOrderRecovery) {
    const int TEST_PORT = 8082; 
    const int TOTAL_PACKETS = 3;
    
    // ------------------------------------------
    // SERVER THREAD 
    // ------------------------------------------
    std::thread server([&]() {
        int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(TEST_PORT);
        bind(sockfd, (const sockaddr *)&serverAddr, sizeof(serverAddr));

        sockaddr_in clientAddr{};
        socklen_t len = sizeof(clientAddr);
        Packet recvPkt{};
        
        uint32_t expectedSequenceNumber = 1;
        std::map<uint32_t, Packet> sequenceNoMap;
        
        int processedCount = 0;

        while(processedCount < TOTAL_PACKETS) {
            ssize_t bytesRecieved = recvfrom(sockfd, &recvPkt, sizeof(recvPkt), 0, (sockaddr *)&clientAddr, &len);
            if (bytesRecieved <= 0) continue;

            uint32_t localSeqNum = ntohl(recvPkt.header.seqNum);
            uint16_t expectedCheckSum = calculateCheckSum(recvPkt.payLoad, sizeof(recvPkt.payLoad));
            
            if(expectedCheckSum != ntohs(recvPkt.checkSum)) continue;

            Packet ackPkt{};
            ackPkt.header.isAck = true;
            ackPkt.header.seqNum = htonl(localSeqNum);
            std::memset(ackPkt.payLoad, 0, sizeof(ackPkt.payLoad));
            ackPkt.checkSum = htons(calculateCheckSum(ackPkt.payLoad, sizeof(ackPkt.payLoad)));
            sendto(sockfd, &ackPkt, sizeof(ackPkt), 0, (const sockaddr *)&clientAddr, len);

            if(localSeqNum == expectedSequenceNumber) {
                processedCount++;
                expectedSequenceNumber++;
                
                while(sequenceNoMap.find(expectedSequenceNumber) != sequenceNoMap.end()) {
                    sequenceNoMap.erase(expectedSequenceNumber);
                    expectedSequenceNumber++;
                    processedCount++;
                }
            } else if(localSeqNum > expectedSequenceNumber) {
                sequenceNoMap[localSeqNum] = recvPkt;
            }
        }
        close(sockfd);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // ------------------------------------------
    // CLIENT THREAD 
    // ------------------------------------------
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(TEST_PORT);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    struct WindowSlot {   
        Packet pkt;
        bool isAcked;
        std::chrono::steady_clock::time_point sendTime;
    };

    const int WINDOW_SIZE = 5;
    WindowSlot window[WINDOW_SIZE];
    uint32_t baseSeq = 1;
    uint32_t nextSeq = 1;
    
    const int TEST_TIMEOUT_MS = 50; 

    while(baseSeq <= TOTAL_PACKETS) {
        if(nextSeq < baseSeq + WINDOW_SIZE && nextSeq <= TOTAL_PACKETS) {
            int index = nextSeq % WINDOW_SIZE;

            window[index].pkt.header.seqNum = htonl(nextSeq);
            std::memset(window[index].pkt.payLoad, 0, sizeof(window[index].pkt.payLoad));
            strcpy(window[index].pkt.payLoad, "Hello!");
            window[index].isAcked = false;
            window[index].sendTime = std::chrono::steady_clock::now();
            window[index].pkt.checkSum = htons(calculateCheckSum(window[index].pkt.payLoad, sizeof(window[index].pkt.payLoad)));

            // FAULT INJECTION SIMULATION: Intentionally skip sending Sequence 2 initially
            if (nextSeq != 2) {
                sendto(sockfd, &window[index].pkt, sizeof(window[index].pkt), 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
            }
            nextSeq++;
        }

        Packet ackRecvdPkt{};
        socklen_t serverAddrLen = sizeof(serverAddr);
        ssize_t bytesRecived = recvfrom(sockfd, &ackRecvdPkt, sizeof(ackRecvdPkt), MSG_DONTWAIT, (sockaddr*)&serverAddr, &serverAddrLen);
        
        if(bytesRecived > 0 && ackRecvdPkt.header.isAck) {
            uint32_t ackSeq = ntohl(ackRecvdPkt.header.seqNum);
            window[ackSeq % WINDOW_SIZE].isAcked = true;
        }
        
        while(window[baseSeq % WINDOW_SIZE].isAcked && baseSeq < nextSeq){
            baseSeq++;
        }

        for(uint32_t seq = baseSeq; seq < nextSeq; seq++){
            int index = seq % WINDOW_SIZE;
            if(!window[index].isAcked){
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - window[index].sendTime).count();
                if(elapsed > TEST_TIMEOUT_MS){
                    sendto(sockfd, &window[index].pkt, sizeof(window[index].pkt), 0, (sockaddr *)&serverAddr, sizeof(serverAddr));
                    window[index].sendTime = std::chrono::steady_clock::now();
                }
            }
        }
    }
    
    close(sockfd);
    server.join();
    
    SUCCEED();
}