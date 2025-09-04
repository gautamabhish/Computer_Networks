#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>

using namespace std;

struct Packet {
    char name[100];
    int number;
};

int main() {
    string clientName;
    int number;
    char mode;

    cout << "Enter your name: ";
    getline(cin, clientName);
    cout << "Enter an integer (1-100): ";
    cin >> number;
    cout << "Use TCP or UDP? (t/u): ";
    cin >> mode;

    if (number < 1 || number > 100) {
        cerr << "Number out of range!\n";
        return 1;
    }

    Packet clientPacket{};
    strncpy(clientPacket.name, clientName.c_str(), sizeof(clientPacket.name) - 1);
    clientPacket.number = number;

    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080);
    inet_pton(AF_INET, "10.210.83.227", &serverAddr.sin_addr);

    if (mode == 't') {
        int sockfd = socket(AF_INET, SOCK_STREAM, 0); // Create TCP socket
        connect(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr));

        send(sockfd, &clientPacket, sizeof(clientPacket), 0);

        Packet serverPacket{};
        size_t totalBytesRead = 0;
        while (totalBytesRead < sizeof(Packet)) {
            int bytesRead = recv(sockfd, ((char*)&serverPacket) + totalBytesRead,
                                 sizeof(Packet) - totalBytesRead, 0);
            if (bytesRead <= 0) break;
            totalBytesRead += bytesRead;
        }

        cout << "Server Name: " << serverPacket.name
             << ", Server Number: " << serverPacket.number
             << ", Sum: " << (number + serverPacket.number) << "\n";

        close(sockfd);
    }
    else {
        int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

        sendto(sockfd, &clientPacket, sizeof(clientPacket), 0,
               (struct sockaddr*)&serverAddr, sizeof(serverAddr));

        Packet serverPacket{};
        socklen_t addr_size = sizeof(serverAddr);
        recvfrom(sockfd, &serverPacket, sizeof(serverPacket), 0,
                 (struct sockaddr*)&serverAddr, &addr_size);

        cout << "Server Name: " << serverPacket.name
             << ", Server Number: " << serverPacket.number
             << ", Sum: " << (number + serverPacket.number) << "\n";

        close(sockfd);
    }

    return 0;
}
