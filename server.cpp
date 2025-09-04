#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctime>
#include <sys/select.h>

using namespace std;

struct Packet {
    char name[100];
    int number;
};

void handleTCPClient(int clientSocket, const string &serverName) {
    Packet clientPacket{};
    if (recv(clientSocket, &clientPacket, sizeof(clientPacket), 0) <= 0) {
        cerr << "[TCP] Failed to receive data.\n";
        close(clientSocket);
        return;
    }

    string clientName(clientPacket.name);
    int clientNumber = clientPacket.number;

    if (clientNumber < 1 || clientNumber > 100) {
        cout << "[TCP] Invalid number. Closing connection.\n";
        close(clientSocket);
        return;
    }

    srand(time(0));
    int serverNumber = (rand() % 100) + 1;

    cout << "[TCP] Client Name: " << clientName
         << ", Server Name: " << serverName
         << ", Client Number: " << clientNumber
         << ", Server Number: " << serverNumber
         << ", Sum: " << (clientNumber + serverNumber) << "\n";

    Packet serverPacket{};
    strncpy(serverPacket.name, serverName.c_str(), sizeof(serverPacket.name) - 1);
    serverPacket.number = serverNumber;

    send(clientSocket, &serverPacket, sizeof(serverPacket), 0);
    close(clientSocket);
}

int main() {
    string serverName = "Server of Rajesh Kumar";
    int tcpSocket, udpSocket;
    struct sockaddr_in serverAddr;

    // TCP socket
    tcpSocket = socket(AF_INET, SOCK_STREAM, 0);
    // UDP socket
    udpSocket = socket(AF_INET, SOCK_DGRAM, 0);

    if (tcpSocket < 0 || udpSocket < 0) {
        cerr << "Socket creation failed.\n";
        return 1;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080);
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    memset(serverAddr.sin_zero, '\0', sizeof(serverAddr.sin_zero));

    // Bind TCP
    if (bind(tcpSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        cerr << "TCP bind failed.\n";
        return 1;
    }
    // Bind UDP
    if (bind(udpSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        cerr << "UDP bind failed.\n";
        return 1;
    }

    if (listen(tcpSocket, 5) < 0) {
        cerr << "Listen failed.\n";
        return 1;
    }

    cout << "Server listening on port 8080 (TCP & UDP)...\n";

    fd_set readfds;
    int maxfd = max(tcpSocket, udpSocket) + 1;

    while (true) {
        FD_ZERO(&readfds);
        FD_SET(tcpSocket, &readfds);
        FD_SET(udpSocket, &readfds);

        if (select(maxfd, &readfds, NULL, NULL, NULL) < 0) {
            cerr << "Select error.\n";
            break;
        }

        if (FD_ISSET(tcpSocket, &readfds)) {
            // Accept TCP client
            struct sockaddr_in clientAddr;
            socklen_t addr_size = sizeof(clientAddr);
            int clientSocket = accept(tcpSocket, (struct sockaddr*)&clientAddr, &addr_size);
            if (clientSocket >= 0) {
                if (fork() == 0) { // Child process
                    close(tcpSocket);
                    handleTCPClient(clientSocket, serverName);
                    exit(0);
                }
                close(clientSocket);
            }
        }

        if (FD_ISSET(udpSocket, &readfds)) {
            // Handle UDP client
            struct sockaddr_in clientAddr;
            socklen_t addr_size = sizeof(clientAddr);
            Packet clientPacket{};
            recvfrom(udpSocket, &clientPacket, sizeof(clientPacket), 0,
                     (struct sockaddr*)&clientAddr, &addr_size);

            string clientName(clientPacket.name);
            int clientNumber = clientPacket.number;

            srand(time(0));
            int serverNumber = (rand() % 100) + 1;

            cout << "[UDP] Client Name: " << clientName
                 << ", Server Name: " << serverName
                 << ", Client Number: " << clientNumber
                 << ", Server Number: " << serverNumber
                 << ", Sum: " << (clientNumber + serverNumber) << "\n";

            Packet serverPacket{};
            strncpy(serverPacket.name, serverName.c_str(), sizeof(serverPacket.name) - 1);
            serverPacket.number = serverNumber;

            sendto(udpSocket, &serverPacket, sizeof(serverPacket), 0,
                   (struct sockaddr*)&clientAddr, addr_size);
        }
    }

    close(tcpSocket);
    close(udpSocket);
    return 0;
}
