#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 5005

int main() {
    int sock;
    sockaddr_in addr{}, sender{};
    socklen_t sender_len = sizeof(sender);
    char buffer[1024];

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;  // ★ Raspberry Piではこれ推奨

    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sock);
        return 1;
    }

    std::cout << "UDP receiver started on port " << PORT << std::endl;

    while (true) {
        ssize_t len = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                               (sockaddr*)&sender, &sender_len);
        buffer[len] = '\0';

        std::cout << "RX: "
                  << inet_ntoa(sender.sin_addr)
                  << ":" << ntohs(sender.sin_port)
                  << " "
                  << buffer << std::endl;
    }

    close(sock);
    return 0;
}
