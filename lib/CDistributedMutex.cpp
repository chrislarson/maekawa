
// CDistributedMutex.cpp
#include "CDistributedMutex.h"

#include <algorithm>
#include <cmath>
#include <sstream>

CDistributedMutex::CDistributedMutex(std::unique_ptr<NetworkLayer> networkLayer,
                                     std::shared_ptr<Logger> logger)
    : state(State::RELEASED),
      hasGivenVote(false),
      votesReceived(0),
      networkLayer(std::move(networkLayer)),
      logger(std::move(logger)),
      myRequest(0, std::vector<int>{}) {}

void CDistributedMutex::GlobalInitialize(
    const int thisHost, const std::vector<InetSocketAddress> &hosts) {
  this->thisHost = thisHost;
  this->hosts = hosts;
  vectorClock.resize(hosts.size(), 0);

  // Create voting groups.
  const auto groups = createVotingGroups(hosts);
  votingGroupHosts = groups[thisHost];
  votesByHost.resize(hosts.size(), 0);

  if (thisHost == 0) {
    logVotingGroups(groups);
  }

  // Establish networking.
  networkLayer->initialize(thisHost, hosts);
  networkLayer->startListening(
      [this](const int host, const std::string &message) {
        messageHandler(host, message);
      });
}

void CDistributedMutex::MLockMutex() {
  std::unique_lock<std::shared_mutex> lock(stateMutex);
  state.store(State::WANTED);

  logger->log(Logger::Level::DEBUG,
              "Attempting to enter critical section. Vector clock: " +
                  vectorClockToString());

  votesReceived.store(0);
  std::fill(votesByHost.begin(), votesByHost.end(), 0);

  // Send request.
  myRequest = sendRequest();

  stateCV.wait(lock, [this] {
    logger->log(Logger::Level::DEBUG,
                "Votes received: " + std::to_string(votesReceived.load()) +
                    "/" + std::to_string(votingGroupHosts.size()));
    return votesReceived.load() == votingGroupHosts.size() &&
           std::all_of(
               votingGroupHosts.begin(), votingGroupHosts.end(),
               [this](const int host) { return votesByHost[host] > 0; });
  });

  state.store(State::HELD);
  logger->log(
      Logger::Level::DEBUG,
      "Entering critical section. Vector clock: " + vectorClockToString());
}

void CDistributedMutex::MReleaseMutex() {
  std::unique_lock<std::shared_mutex> lock(stateMutex);
  state.store(State::RELEASED);

  logger->log(
      Logger::Level::DEBUG,
      "Releasing critical section. Vector clock: " + vectorClockToString());

  sendRelease();  // Send release to other members of group.
  handleRelease(thisHost, vectorClock);  // Handle release.
  logRequestQueue();
}

void CDistributedMutex::handleRequest(
    const int fromHost, const std::vector<int> &fromHostVectorClock) {
  std::unique_lock<std::shared_mutex> lock(vectorClockMutex);
  updateVectorClock(fromHostVectorClock);

  const auto req = Request(fromHost, fromHostVectorClock);

  logger->log(Logger::Level::DEBUG,
              "Received request from host " + std::to_string(fromHost) +
                  ". Current state: " + stateToString(state.load()) +
                  ". Vector clock: " + vectorClockToString());

  if (state.load() == State::HELD || hasGivenVote.load()) {
    logger->log(Logger::Level::DEBUG,
                "Queueing request from " + std::to_string(fromHost));
    std::unique_lock<std::shared_mutex> queueLock(requestQueueMutex);
    requestQueue.push(req);
    logRequestQueue();
  } else {
    lock.unlock();  // Release lock before sending message
    sendOK(fromHost);
    hasGivenVote.store(true);
  }
}

void CDistributedMutex::handleRelease(
    const int fromHost, const std::vector<int> &fromHostVectorClock) {
  std::unique_lock<std::shared_mutex> lock(vectorClockMutex);
  updateVectorClock(fromHostVectorClock);
  lock.unlock();

  std::unique_lock<std::shared_mutex> queueLock(requestQueueMutex);
  if (requestQueue.empty()) {
    hasGivenVote.store(false);
  } else {
    const auto nextRequest = requestQueue.top();
    requestQueue.pop();
    queueLock.unlock();
    sendOK(nextRequest.hostId);
    hasGivenVote.store(true);
    logger->log(Logger::Level::DEBUG,
                "Received release from " + std::to_string(fromHost) +
                    ". Sent OK to host " + std::to_string(nextRequest.hostId));
  }
}

void CDistributedMutex::handleOK(const int fromHost,
                                 const std::vector<int> &fromHostVectorClock) {
  std::unique_lock<std::shared_mutex> lock(vectorClockMutex);
  updateVectorClock(fromHostVectorClock);
  lock.unlock();

  votesByHost[fromHost]++;
  votesReceived.fetch_add(1);
  stateCV.notify_all();
}

void CDistributedMutex::updateVectorClock(const std::vector<int> &otherClock) {
  for (size_t i = 0; i < vectorClock.size(); ++i) {
    vectorClock[i] =
        std::max(vectorClock[i], otherClock.empty() ? 0 : otherClock[i]);
  }
  logVectorClock();
}

bool CDistributedMutex::isRequestPrioritized(const Request &req) const {
  for (size_t i = 0; i < vectorClock.size(); ++i) {
    if (req.vectorClock[i] < vectorClock[i]) return true;
    if (req.vectorClock[i] > vectorClock[i]) return false;
  }
  return req.hostId < thisHost;
}

CDistributedMutex::Request CDistributedMutex::sendRequest() {
  const std::string vectorClockStr = incrementVectorClock();
  auto request = Request(thisHost, vectorClock);
  for (const int targetHost : votingGroupHosts) {
    if (targetHost != thisHost) {
      std::string message =
          std::to_string(thisHost) + " REQUEST " + vectorClockStr;
      networkLayer->sendMessage(targetHost, message);
      logger->log(Logger::Level::DEBUG, "Sent REQUEST to host " +
                                            std::to_string(targetHost) + ": " +
                                            message);
    } else {
      handleRequest(thisHost, vectorClock);
    }
  }
  return request;
}

void CDistributedMutex::sendRelease() {
  const std::string vectorClockStr = incrementVectorClock();
  for (const int targetHost : votingGroupHosts) {
    if (targetHost != thisHost) {
      std::string message =
          std::to_string(thisHost) + " RELEASE " + vectorClockStr;
      networkLayer->sendMessage(targetHost, message);
    } else {
      // Note, do not handle release here - I take care of it in release itself.
    }
  }
  logger->log(Logger::Level::DEBUG,
              "Sent RELEASE. Vector clock: " + vectorClockStr);
}

void CDistributedMutex::sendOK(const int targetHost) {
  const std::string vectorClockStr = incrementVectorClock();
  if (targetHost != thisHost) {
    const std::string message =
        std::to_string(thisHost) + " OK " + vectorClockStr;
    networkLayer->sendMessage(targetHost, message);
  } else {
    handleOK(thisHost, vectorClock);
  }
  hasGivenVote.store(true);
}

void CDistributedMutex::messageHandler(const int host,
                                       const std::string &message) {
  const std::vector<std::string> parts = split(message, ' ');
  if (parts.size() < 3) {
    logger->log(Logger::Level::ERROR, "Received malformed message: " + message);
    return;
  }

  const int senderHost = std::stoi(parts[0]);
  const std::string &messageType = parts[1];
  const std::vector<int> receivedVectorClock = parseVectorClock(parts[2]);

  if (receivedVectorClock.empty()) {
    logger->log(Logger::Level::ERROR,
                "Failed to parse vector clock: " + parts[2]);
    return;
  }

  if (messageType == "RELEASE") {
    handleRelease(senderHost, receivedVectorClock);
  } else if (messageType == "OK") {
    handleOK(senderHost, receivedVectorClock);
  } else if (messageType == "REQUEST") {
    handleRequest(senderHost, receivedVectorClock);
  } else {
    logger->log(Logger::Level::ERROR, "Unknown message type: " + messageType);
  }
}

std::string CDistributedMutex::stateToString(const State s) {
  switch (s) {
    case State::RELEASED:
      return "RELEASED";
    case State::WANTED:
      return "WANTED";
    case State::HELD:
      return "HELD";
    default:
      return "UNKNOWN";
  }
}

void CDistributedMutex::logRequestQueue() const {
  std::stringstream ss;
  ss << "Request queue: ";
  auto tmp_q = requestQueue;
  while (!tmp_q.empty()) {
    auto q_element = tmp_q.top();
    ss << q_element.hostId << " - (";
    for (int i = 0; i < q_element.vectorClock.size(); i++) {
      ss << q_element.vectorClock[i];
      if (i < q_element.vectorClock.size() - 1) {
        ss << ",";
      }
    }
    ss << ") | ";
    tmp_q.pop();
  }
  logger->log(Logger::Level::DEBUG, ss.str());
}

std::string CDistributedMutex::incrementVectorClock() {
  std::unique_lock<std::shared_mutex> lock(vectorClockMutex);
  vectorClock[thisHost]++;
  const std::string vectorClockStr = vectorClockToString();
  lock.unlock();
  logVectorClock();
  return vectorClockStr;
}

std::string CDistributedMutex::vectorClockToString() const {
  std::stringstream ss;
  ss << "(";
  for (size_t i = 0; i < vectorClock.size(); ++i) {
    ss << vectorClock[i];
    if (i < vectorClock.size() - 1) {
      ss << ",";
    }
  }
  ss << ")";
  return ss.str();
}

void CDistributedMutex::logVectorClock() const {
  std::stringstream ss;
  ss << hosts[thisHost].host << ":(";
  for (size_t i = 0; i < vectorClock.size(); ++i) {
    ss << vectorClock[i];
    if (i < vectorClock.size() - 1) {
      ss << ",";
    }
  }
  ss << ")";
  logger->log(Logger::Level::INFO, ss.str());
}

void CDistributedMutex::MCleanup() const {
  std::stringstream ss;
  ss << "Votes by host: ";
  for (const auto &voteCount : votesByHost) {
    ss << voteCount << ",";
  }
  logger->log(Logger::Level::DEBUG, ss.str());
}

std::vector<std::set<int>> CDistributedMutex::createVotingGroups(
    const std::vector<InetSocketAddress> &hosts) {
  // Grid approach from paper.
  const int numProcesses = static_cast<int>(hosts.size());
  const int k = std::ceil(std::sqrt(numProcesses));
  std::vector<std::set<int>> groups(numProcesses);
  for (int i = 0; i < numProcesses; ++i) {
    for (int j = 0; j < k; ++j) {
      if (int member = i / k * k + j; member < numProcesses) {
        groups[i].insert(member);
      }
    }
    for (int j = 0; j < k; ++j) {
      if (int member = (i % k) + j * k; member < numProcesses && member != i) {
        groups[i].insert(member);
      }
    }
  }
  return groups;
}

std::vector<int> CDistributedMutex::parseVectorClock(
    const std::string &s) const {
  std::vector<int> result;
  std::string cleaned = s;
  // Remove parentheses if they exist
  if (s.front() == '(' && s.back() == ')') {
    cleaned = s.substr(1, s.length() - 2);
  }
  const std::vector<std::string> parts = split(cleaned, ',');
  for (const auto &part : parts) {
    try {
      result.push_back(std::stoi(part));
    } catch (const std::invalid_argument &) {
      logger->log(Logger::Level::ERROR,
                  "Invalid vector clock component: " + part);
      return {};
    } catch (const std::out_of_range &) {
      logger->log(Logger::Level::ERROR,
                  "Vector clock component out of range: " + part);
      return {};
    }
  }
  return result;
}

std::vector<std::string> CDistributedMutex::split(const std::string &s,
                                                  char delimiter) {
  std::vector<std::string> tokens;
  std::string token;
  std::istringstream tokenStream(s);
  while (std::getline(tokenStream, token, delimiter)) {
    tokens.push_back(token);
  }
  return tokens;
}

void CDistributedMutex::logVotingGroups(
    const std::vector<std::set<int>> &groups) const {
  std::stringstream ss;
  ss << "Voting Groups:\n";

  for (size_t i = 0; i < groups.size(); ++i) {
    ss << "Host " << i << " voting group: {";
    const auto &group = groups[i];
    for (auto it = group.begin(); it != group.end(); ++it) {
      ss << *it;
      if (std::next(it) != group.end()) {
        ss << ", ";
      }
    }
    ss << "}\n";
  }

  logger->log(Logger::Level::INFO, ss.str());
}
