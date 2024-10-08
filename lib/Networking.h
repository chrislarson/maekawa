#ifndef NETWORKING_H
#define NETWORKING_H

#include <atomic>
#include <string>
#include <thread>
#include <vector>
#include <functional>

struct InetSocketAddress {
    std::string host;
    int port;
};

class NetworkLayer {
public:
    NetworkLayer();

    ~NetworkLayer();

    void initialize(int thisHost, const std::vector<InetSocketAddress> &hosts);

    void sendMessage(int toHost, const std::string &message) const;

    void startListening(const std::function<void(int, const std::string &)> &messageHandler);

    void stop();

    // Disable copy constructor and assignment operator
    NetworkLayer(const NetworkLayer &) = delete;

    NetworkLayer &operator=(const NetworkLayer &) = delete;

private:
    int thisHost;
    std::vector<InetSocketAddress> hosts;
    std::vector<int> clientSockets;
    int serverSocket;
    std::thread listenerThread;
    std::atomic<bool> running;


    void listenForMessages(const std::function<void(int, const std::string &)> &messageHandler) const;
};

#endif  // NETWORKING_H
