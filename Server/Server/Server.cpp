#include <iostream>
#include <winsock2.h>
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <fstream>
#include <locale>
#include <codecvt>

#define MAX_CLIENTS 256
#define BUF_SIZE 1024

#pragma comment(lib, "ws2_32.lib")

struct ClientData {
    SOCKET socket;
    std::string username;
};

std::mutex clientsMutex;
std::vector<ClientData> clients;

std::ofstream logFile("chat_logs.txt", std::ios::app);

std::string toUtf8(const std::wstring& wideStr) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(wideStr);
}

std::wstring toWideString(const std::string& str) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.from_bytes(str);
}

void logMessage(const std::string& message) {
    if (logFile.is_open()) {
        logFile << message << std::endl;
    }
}

void handleClient(SOCKET clientSocket) {
    char buffer[BUF_SIZE];
    int bytesReceived;

    bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesReceived <= 0) {
        std::cerr << "Failed to receive username" << std::endl;
        closesocket(clientSocket);
        return;
    }
    buffer[bytesReceived] = '\0';
    std::string username(buffer);

    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        clients.push_back({ clientSocket, username });
    }

    std::string joinMessage = "[SERVER]: User \"" + username + "\" has joined\n";
    std::cout << joinMessage;
    logMessage(joinMessage);  // Log the join message
    send(clientSocket, joinMessage.c_str(), joinMessage.size(), 0);

    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        for (const auto& client : clients) {
            if (client.socket != clientSocket) {
                send(client.socket, joinMessage.c_str(), joinMessage.size(), 0);
            }
        }
    }

    while ((bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0)) > 0) {
        buffer[bytesReceived] = '\0';
        std::string message(buffer);

        if (message == "/users") {
            std::string userList = "[SERVER]: Active users:\n";
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                for (const auto& client : clients) {
                    userList += client.username + "\n";
                }
            }
            send(clientSocket, userList.c_str(), userList.size(), 0);
        }
        else if (message == "/exit") {
            break;
        }
        else {
            message = "[" + username + "]: " + message;
            std::cout << message;
            logMessage(message);  // Log the user message

            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                for (const auto& client : clients) {
                    if (client.socket != clientSocket) {
                        send(client.socket, message.c_str(), message.size(), 0);
                    }
                }
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        clients.erase(std::remove_if(clients.begin(), clients.end(),
            [clientSocket](const ClientData& client) { return client.socket == clientSocket; }),
            clients.end());
    }

    std::string leaveMessage = "[SERVER]: User \"" + username + "\" left the chat\n";
    std::cout << leaveMessage;
    logMessage(leaveMessage);  // Log the leave message

    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        for (const auto& client : clients) {
            send(client.socket, leaveMessage.c_str(), leaveMessage.size(), 0);
        }
    }

    closesocket(clientSocket);
}

void startServer(int port) {
    WSADATA wsaData;
    SOCKET serverSocket, clientSocket;
    struct sockaddr_in serverAddr, clientAddr;
    int clientAddrSize = sizeof(clientAddr);

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return;
    }

    serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Error creating server socket" << std::endl;
        WSACleanup();
        return;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed" << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return;
    }

    if (listen(serverSocket, MAX_CLIENTS) == SOCKET_ERROR) {
        std::cerr << "Listen failed" << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return;
    }

    std::cout << "Server listening on port " << port << std::endl;

    while (true) {
        clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrSize);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Accept failed" << std::endl;
            continue;
        }

        std::thread(handleClient, clientSocket).detach();
    }

    closesocket(serverSocket);
    WSACleanup();
}

int main() {
    startServer(12345);
    return 0;
}
