#include "common/logger.hpp"

#include <ctime>
#include <iomanip>
#include <iostream>

namespace common {

Logger& Logger::instance() {
  static Logger logger;
  return logger;
}

void Logger::info(const std::string& msg) { log("INFO", msg); }
void Logger::warn(const std::string& msg) { log("WARN", msg); }
void Logger::error(const std::string& msg) { log("ERROR", msg); }

void Logger::log(const std::string& level, const std::string& msg) {
  std::lock_guard<std::mutex> lk(mu_);
  const std::time_t now = std::time(nullptr);
  std::tm tm = *std::localtime(&now);
  std::cout << std::put_time(&tm, "%F %T") << " [" << level << "] " << msg << std::endl;
}

}  // namespace common
