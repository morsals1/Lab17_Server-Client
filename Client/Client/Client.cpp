#include <iostream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>

#pragma comment(lib, "Ws2_32.lib")

#define PORT 12345

void readMessages(SOCKET clientSocket) {
    char buffer[1024];
    int bytesReceived;
    while ((bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytesReceived] = '\0';
        std::cout << buffer << std::endl;
    }
    std::cout << "[SERVER]: Connection lost.\n";
}

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET clientSocket;
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);

    std::string serverIp;
    bool connected = false;

    while (!connected) {
        std::cout << "Enter server IP: ";
        std::getline(std::cin, serverIp);

        if (InetPtonA(AF_INET, serverIp.c_str(), &serverAddr.sin_addr) != 1) {
            std::cerr << "Invalid IP address format! Please enter again.\n";
            continue;
        }

        clientSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Error creating socket!\n";
            WSACleanup();
            return 1;
        }

        if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cerr << "Connection failed! Please enter a valid IP.\n";
            closesocket(clientSocket);
            continue;
        }

        connected = true;
    }

    std::string username;
    char buffer[1024];
    int bytesReceived;

    do {
        std::cout << "Enter your username: ";
        std::getline(std::cin, username);
        send(clientSocket, username.c_str(), username.length(), 0);

        bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            std::cout << buffer << std::endl;
        }
    } while (std::string(buffer).find("already taken") != std::string::npos);

    std::thread readerThread(readMessages, clientSocket);

    std::string message;
    while (true) {
        std::getline(std::cin, message);
        if (message == "/exit") {
            send(clientSocket, message.c_str(), message.length(), 0);
            break;
        }
        send(clientSocket, message.c_str(), message.length(), 0);
    }

    closesocket(clientSocket);
    WSACleanup();
    readerThread.join();

    return 0;
}
