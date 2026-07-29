#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "protocol.hpp"
#include <random>

int main(){
    // 1. UDP socket:
    // Domain : AF_INET for IPV4
    // Type: SOCK_DGRAM for UDP
    // Protocol : 0(default)
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if(sockfd < 0){
        perror("Socket creation failed");
        return 1;
    }

    // 2. Server adress struct
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;            // AF_INET for IPV4
    serverAddr.sin_addr.s_addr = INADDR_ANY;    // listen to any local IP
    serverAddr.sin_port = htons(8080);          // htons : host to network byte order

    // 3. Bind the socket to port
    // socket: sockfd
    // server address: server address
    // len of server address: sizeof(serverAddr)
    if( bind(sockfd, (const sockaddr *)&serverAddr, sizeof(serverAddr)) < 0){
        perror("bind failed");
        return 1;
    }

    // 4. Recieve from:
    // create character array buffer
    // send it recvfrom()
    sockaddr_in clientAddr{};
    socklen_t len = sizeof(clientAddr);

    Packet recvPkt;

    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, 100);
    const int DROP_PROBABILITY = 30;

    while(true){
        ssize_t bytesRecieved = recvfrom(sockfd, &recvPkt, sizeof(recvPkt), MSG_WAITALL, (sockaddr *)&clientAddr, &len);
        if(bytesRecieved < 0){
            perror("Failed to Recieve message");
            continue;
        }
        uint32_t localSeqNum = ntohl(recvPkt.header.seqNum);
        bool isAcknowledgement = recvPkt.header.isAck;

        if(dist(rng) <= DROP_PROBABILITY){
            std::cout << "[FAULT INJECTION] Intentionally dropped packet Sequence no : " << localSeqNum << std::endl;
            continue;
        }

        std::cout << "Recived Seq : " << localSeqNum << std::endl;
        std::cout << "Message : " << recvPkt.payLoad << std::endl;

        Packet ackPkt;
        ackPkt.header.isAck = true;
        ackPkt.header.seqNum = htonl(localSeqNum);
        std::memset(ackPkt.payLoad, 0, sizeof(ackPkt.payLoad));
        if(sendto(sockfd, &ackPkt, sizeof(ackPkt), 0, (const sockaddr *)&clientAddr, len) < 0)
        {
            perror("Failed to send acknowledgement");
            return 1;
        }
    }
    close(sockfd);
    return 0;
}