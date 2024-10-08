#include <vector>
#include <cstdlib>
#include <iostream>
#include "CDistributedMutex.h"
#include "Networking.h"
#include "Logger.h"
#include <random>

[[noreturn]] int main(const int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <host_id> <num_hosts> <host_port>" << std::endl;
        exit(1);
    }

    const int thisHost = std::stoi(argv[1]);
    const int numHosts = std::stoi(argv[2]);
    const int hostPort = std::stoi(argv[3]);
    const std::string logLevel = std::getenv("LOG_LEVEL") ? std::getenv("LOG_LEVEL") : "INFO";

    std::vector<InetSocketAddress> hosts;
    hosts.reserve(numHosts);
    for (int i = 0; i < numHosts; ++i) {
        hosts.emplace_back(InetSocketAddress{"172.18.0." + std::to_string(i + 2), 9100 + i});
    }

    auto networkLayer = std::make_unique<NetworkLayer>();
    const auto logger = std::make_shared<Logger>("distributed_mutex.log", Logger::stringToLevel(logLevel));
    auto maekawa = CDistributedMutex(std::move(networkLayer), logger);

    maekawa.GlobalInitialize(thisHost, hosts);
    std::this_thread::sleep_for(std::chrono::seconds(3)); // 3 for all to start

    while (true) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(6, 30);
        int sleep_time = dis(gen);
        logger->log(Logger::Level::INFO, "-> Host " + std::to_string(thisHost) + " sleeping for " + std::to_string(sleep_time) + " seconds before next CS request.");
        std::this_thread::sleep_for(std::chrono::seconds(sleep_time));

        maekawa.MLockMutex();
        logger->log(Logger::Level::INFO, "**** Host " + std::to_string(thisHost) + " - ENTERING CRITICAL SECTION. ****");
        std::this_thread::sleep_for(std::chrono::seconds(5));
        logger->log(Logger::Level::INFO, "**** Host " + std::to_string(thisHost) + " - EXITING CRITICAL SECTION. ****");
        maekawa.MReleaseMutex();
        maekawa.MCleanup();
    }
}