#include "Networking.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <thread>


NetworkLayer::NetworkLayer() : thisHost(-1), serverSocket(-1), running(false) {
}

NetworkLayer::~NetworkLayer() { stop(); }

void NetworkLayer::initialize(const int thisHost,
                              const std::vector<InetSocketAddress> &hosts) {
    this->thisHost = thisHost;
    this->hosts = hosts;
}

void NetworkLayer::startListening(const std::function<void(int, const std::string &)> &messageHandler
) {
    running = true;
    listenerThread =
            std::thread(&NetworkLayer::listenForMessages, this, messageHandler);
}

void NetworkLayer::stop() {
    running = false;
    if (listenerThread.joinable()) {
        listenerThread.join();
    }
    for (const int socket: clientSockets) {
        close(socket);
    }
    if (serverSocket != -1) {
        close(serverSocket);
    }
}


void NetworkLayer::sendMessage(const int toHost, const std::string &message) const {
    const int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Error creating socket" << std::endl;
        return;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(hosts[toHost].port);

    if (inet_pton(AF_INET, hosts[toHost].host.c_str(), &serverAddress.sin_addr) <=
        0) {
        std::cerr << "Invalid address for host " << toHost << std::endl;
        close(sock);
        return;
    }

    if (connect(sock, reinterpret_cast<sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        std::cerr << "Connection failed to host " << toHost << std::endl;
        close(sock);
        return;
    }

    send(sock, message.c_str(), message.size(), 0);
    close(sock);
}

void NetworkLayer::listenForMessages(const std::function<void(int, const std::string &)> &messageHandler) const {
    const int sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(hosts[thisHost].port);

    if (bind(sock, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0) {
        std::cerr << "Bind failed for host " << thisHost << std::endl;
        return;
    }

    listen(sock, 10);

    while (running) {
        sockaddr_in clientAddress{};
        socklen_t clientAddressLength = sizeof(clientAddress);
        const int newSocket = accept(sock, reinterpret_cast<struct sockaddr *>(&clientAddress), &clientAddressLength);

        if (newSocket < 0) {
            std::cerr << "Error accepting connection for host " << thisHost
                    << std::endl;
            continue;
        }

        char buffer[1024] = {};
        const size_t bytesRead = recv(newSocket, buffer, 1024, 0);
        if (bytesRead <= 0) {
            std::cerr << "Error reading data from connection for host " << thisHost
                    << std::endl;
            close(newSocket);
            continue;
        }

        std::string message(buffer, bytesRead);
        messageHandler(thisHost, message);
        close(newSocket);
    }
}
