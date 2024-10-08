// Logger.cpp
#include "Logger.h"

#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <mutex>

Logger::Logger(const std::string &filename, Level minLevel)
  : minLevel(minLevel), logFile(filename, std::ios::app) {
}

Logger::~Logger() {
  if (logFile.is_open()) {
    logFile.close();
  }
}

void Logger::log(const Level level, const std::string &message) {
  if (level >= minLevel) {
    std::lock_guard<std::mutex> lock(logMutex);
    const auto now = std::chrono::system_clock::now();
    const auto now_c = std::chrono::system_clock::to_time_t(now);
    std::cout << std::put_time(std::localtime(&now_c), "%F %T") << " ["
        << levelToString(level) << "] " << message << std::endl;

    // Millisecond timing.
    // const auto duration = now.time_since_epoch();
    // const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    // std::cout << milliseconds << " " << levelToString(level) << ": " << message << std::endl;
  }
}

const char *Logger::levelToString(const Level level) {
  switch (level) {
    case Level::DEBUG:
      return "DEBUG";
    case Level::INFO:
      return "INFO";
    case Level::WARNING:
      return "WARNING";
    case Level::ERROR:
      return "ERROR";
    default:
      return "UNKNOWN";
  }
}

Logger::Level Logger::stringToLevel(const std::string &levelStr) {
  if (levelStr == "DEBUG") return Logger::Level::DEBUG;
  if (levelStr == "INFO") return Logger::Level::INFO;
  if (levelStr == "WARN") return Logger::Level::WARNING;
  if (levelStr == "ERROR") return Logger::Level::ERROR;
  return Level::INFO; // default
}
