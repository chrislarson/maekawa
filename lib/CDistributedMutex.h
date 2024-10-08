// CDistributedMutex.h
#pragma once

#include <vector>
#include <queue>
#include <shared_mutex>
#include <condition_variable>
#include <set>

#include "CDistributedMutex.h"
#include "Networking.h"
#include "Logger.h"

class CDistributedMutex {
public:
    CDistributedMutex(std::unique_ptr<NetworkLayer> networkLayer, std::shared_ptr<Logger> logger);


    void GlobalInitialize(int thisHost, const std::vector<InetSocketAddress> &hosts);

    void MLockMutex();

    void MReleaseMutex();

    void MCleanup() const;

    static std::vector<std::set<int> > createVotingGroups(
        const std::vector<InetSocketAddress> &hosts);

    std::vector<int> parseVectorClock(const std::string &s) const;

    static std::vector<std::string> split(const std::string &s, char delimiter);

    void logVotingGroups(const std::vector<std::set<int>> &groups) const;

private:
    enum class State { RELEASED, WANTED, HELD };

    struct Request {
        int hostId;
        std::vector<int> vectorClock;
        std::chrono::steady_clock::time_point timestamp;

        Request(const int id, const std::vector<int> &clock)
            : hostId(id), vectorClock(clock), timestamp(std::chrono::steady_clock::now()) {
        }

        bool operator<(const Request &other) const {
            for (size_t i = 0; i < other.vectorClock.size(); ++i) {
                if (vectorClock[i] < other.vectorClock[i]) return true;
                if (vectorClock[i] > other.vectorClock[i]) return false;
            }
            // If vector clocks are concurrent, break tie with hostId
            return hostId < other.hostId;
        }
    };

    struct RequestComparator {
        bool operator()(const Request &lhs, const Request &rhs) const {
            // First, compare vector clocks
            for (size_t i = 0; i < lhs.vectorClock.size(); ++i) {
                if (lhs.vectorClock[i] < rhs.vectorClock[i]) return false;
                if (lhs.vectorClock[i] > rhs.vectorClock[i]) return true;
            }

            // If vector clocks are concurrent, break tie with hostId
            return lhs.hostId > rhs.hostId;
        }
    };

    std::atomic<State> state;
    std::shared_mutex stateMutex;
    std::condition_variable_any stateCV;

    std::vector<int> vectorClock;
    std::shared_mutex vectorClockMutex;

    std::atomic<bool> hasGivenVote;
    std::atomic<int> votesReceived;

    std::set<int> votingGroupHosts;
    std::vector<int> votesByHost;

    std::priority_queue<Request, std::vector<Request>, RequestComparator> requestQueue;;
    std::shared_mutex requestQueueMutex;

    std::unique_ptr<NetworkLayer> networkLayer;
    std::shared_ptr<Logger> logger;

    int thisHost{};
    std::vector<InetSocketAddress> hosts;

    Request myRequest;

    Request sendRequest();

    void sendRelease();

    void sendOK(int targetHost);

    void handleRequest(int fromHost, const std::vector<int> &fromHostVectorClock);

    void handleRelease(int fromHost, const std::vector<int> &fromHostVectorClock);

    void handleOK(int fromHost, const std::vector<int> &fromHostVectorClock);

    void messageHandler(int host, const std::string &message);

    void updateVectorClock(const std::vector<int> &otherClock);

    bool isRequestPrioritized(const Request &req) const;

    void logRequestQueue() const;

    std::string incrementVectorClock();

    std::string vectorClockToString() const;

    void logVectorClock() const;

    static std::string stateToString(State s);

    static constexpr std::chrono::seconds VOTE_TIMEOUT{30}; // 30-second timeout for votes
};
