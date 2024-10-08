// Logger.h
#ifndef LOGGER_H
#define LOGGER_H

#include <fstream>
#include <mutex>

class Logger {
public:
  enum class Level { DEBUG, INFO, WARNING, ERROR };

  explicit Logger(const std::string &filename, Level minLevel = Level::INFO);

  ~Logger();

  void log(Level level, const std::string &message);

  static Level stringToLevel(const std::string &levelStr);

  // Disable copy constructor and assignment operator
  Logger(const Logger &) = delete;

  Logger &operator=(const Logger &) = delete;

private:
  Level minLevel;
  std::ofstream logFile;
  std::mutex logMutex;

  static const char *levelToString(Level level);
};

#endif // LOGGER_H
