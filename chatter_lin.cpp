// solip_chat.cpp
#include <iostream>
#include <thread>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <sstream>

#define RED "\033[31m"
#define RESET "\033[0m"

using namespace std;

// Packet structure
struct Packet {
    char type = 0;  // 0 = public, 1 = private
    char name[80];
    char message[1024];
};

string userName;
unordered_map<string, sockaddr_in> userDirectory; // username -> address
mutex coutMutex;
mutex userDirMutex;

// Trim leading/trailing whitespace and nulls
static void trim_string(string &s) {
    while (!s.empty() && (s.back() == '\0' || isspace((unsigned char)s.back()))) s.pop_back();
    size_t start = 0;
    while (start < s.size() && (s[start] == '\0' || isspace((unsigned char)s[start]))) start++;
    if (start > 0) s.erase(0, start);
}

// Broadcast JOIN message
void sendJoin(int sockfd, const sockaddr_in &broadcastAddr) {
    Packet p{};
    memset(&p, 0, sizeof(p));
    strncpy(p.name, userName.c_str(), sizeof(p.name)-1);
    strncpy(p.message, "/JOIN", sizeof(p.message)-1);
    sendto(sockfd, &p, sizeof(p), 0, (const sockaddr*)&broadcastAddr, sizeof(broadcastAddr));
}

// Send NAME_TAKEN warning
void sendNameTaken(int sockfd, const sockaddr_in &dest) {
    Packet p{};
    memset(&p, 0, sizeof(p));
    strncpy(p.name, "SERVER", sizeof(p.name)-1);
    strncpy(p.message, "/NAME_TAKEN", sizeof(p.message)-1);
    sendto(sockfd, &p, sizeof(p), 0, (const sockaddr*)&dest, sizeof(dest));
}

// Safe console print
void safePrintLn(const string &s) {
    lock_guard<mutex> lock(coutMutex);
    cout << "\r" << s << endl;
    cout << "> " << flush;
}

// Receiving messages
void receiveMessages(int sockfd) {
    Packet packet{};
    sockaddr_in peerAddr{};
    socklen_t addrLen = sizeof(peerAddr);

    while (true) {
        memset(&packet, 0, sizeof(packet));
        int bytes = recvfrom(sockfd, &packet, sizeof(packet), 0,
                             (sockaddr*)&peerAddr, &addrLen);
        if (bytes <= 0) continue;

        packet.name[79] = '\0';
        packet.message[1023] = '\0';
        string rawName(packet.name), rawMsg(packet.message);
        trim_string(rawName);
        trim_string(rawMsg);
        if (rawName.empty()) continue;

        // Control messages
        if (rawMsg == "/JOIN") {
            lock_guard<mutex> lock(userDirMutex);
            auto it = userDirectory.find(rawName);
            if (it == userDirectory.end()) {
                userDirectory[rawName] = peerAddr;
                safePrintLn("[JOIN] " + rawName);
            } else if (it->second.sin_addr.s_addr != peerAddr.sin_addr.s_addr) {
                sendNameTaken(sockfd, peerAddr);
                safePrintLn("[CONFLICT] Username '" + rawName + "' already taken");
            } else {
                it->second = peerAddr; // refresh
            }
            continue;
        }

        if (rawMsg == "/NAME_TAKEN" && rawName == "SERVER") {
            safePrintLn("[SERVER] Your username '" + userName + "' is already taken. Use /rename NEW_NAME");
            continue;
        }

        // Normal messages
        {
            lock_guard<mutex> coutLock(coutMutex);
            if (packet.type == 1) {
                cout << RED << "[PRIVATE] " << rawName << ": " << rawMsg << RESET << endl;
            } else {
                cout << rawName << ": " << rawMsg << endl;
            }
            cout << "> " << flush;
        }

        // Update user directory
        lock_guard<mutex> lock(userDirMutex);
        auto it = userDirectory.find(rawName);
        if (it == userDirectory.end()) {
            userDirectory[rawName] = peerAddr;
        } else if (it->second.sin_addr.s_addr != peerAddr.sin_addr.s_addr) {
            sendNameTaken(sockfd, peerAddr);
        } else {
            it->second = peerAddr;
        }
    }
}

// Sending messages
void sendMessages(int sockfd, sockaddr_in broadcastAddr) {
    string msg;
    Packet packet{};
    memset(&packet, 0, sizeof(packet));
    strncpy(packet.name, userName.c_str(), sizeof(packet.name)-1);

    while (true) {
        {
            lock_guard<mutex> lock(coutMutex);
            cout << "> " << flush;
        }

        if (!getline(cin, msg)) {
            close(sockfd);
            exit(0);
        }
        if (msg.empty()) continue;

        // Commands
        if (msg.rfind("/rename ", 0) == 0) {
            string newName = msg.substr(8);
            trim_string(newName);
            if (newName.empty()) { safePrintLn("[CLIENT] Usage: /rename NEW_NAME"); continue; }
            userName = newName;
            strncpy(packet.name, userName.c_str(), sizeof(packet.name)-1);
            sendJoin(sockfd, broadcastAddr);
            safePrintLn("[CLIENT] Renamed to '" + userName + "' and broadcasted JOIN");
            continue;
        }

        if (msg == "/quit") {
            close(sockfd);
            exit(0);
        }

        memset(packet.message, 0, sizeof(packet.message));
        packet.type = 0; // default public

        // Private messages
        if (msg[0] == '@') {
            size_t spacePos = msg.find(' ');
            if (spacePos != string::npos) {
                string targetUser = msg.substr(1, spacePos - 1);
                string privateMsg = msg.substr(spacePos + 1);

                lock_guard<mutex> lock(userDirMutex);
                auto it = userDirectory.find(targetUser);
                if (it != userDirectory.end()) {
                    packet.type = 1;
                    strncpy(packet.message, privateMsg.c_str(), sizeof(packet.message)-1);
                    sendto(sockfd, &packet, sizeof(packet), 0, (sockaddr*)&it->second, sizeof(it->second));
                    cout << RED << "[PRIVATE to " << targetUser << "] " << privateMsg << RESET << endl;
                } else {
                    safePrintLn("[CLIENT] No such user: " + targetUser);
                }
            }
            continue;
        }

        // Public broadcast
        strncpy(packet.message, msg.c_str(), sizeof(packet.message)-1);
        sendto(sockfd, &packet, sizeof(packet), 0, (const sockaddr*)&broadcastAddr, sizeof(broadcastAddr));
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) { cerr << "Usage: " << argv[0] << " <peer_ip_or_broadcast>\n"; return 1; }

    cout << "Enter your name: ";
    getline(cin, userName);
    trim_string(userName);
    if (userName.empty()) { cerr << "Name cannot be empty\n"; return 1; }

    // UDP socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) { perror("socket"); return 1; }

    int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable)) < 0) {
        perror("setsockopt(SO_BROADCAST) failed"); return 1;
    }

    sockaddr_in localAddr{};
    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(7000);
    localAddr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sockfd, (sockaddr*)&localAddr, sizeof(localAddr)) < 0) { perror("bind"); return 1; }

    cout << "Chat started on port 7000.\nType messages. /quit to exit. /rename NEWNAME to change name.\n@username message for private.\n";

    sockaddr_in peerAddr{};
    peerAddr.sin_family = AF_INET;
    peerAddr.sin_port = htons(7000);
    inet_pton(AF_INET, argv[1], &peerAddr.sin_addr);

    sendJoin(sockfd, peerAddr);

    thread t1(receiveMessages, sockfd);
    thread t2(sendMessages, sockfd, peerAddr);

    t1.join();
    t2.join();
    close(sockfd);
    return 0;
}
