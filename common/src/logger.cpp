#include "common/logger.hpp"

#include <iostream>

#include "common/time_utils.hpp"

namespace common {

Logger& Logger::instance() {
  static Logger logger;
  return logger;
}

void Logger::info(const std::string& msg) { log("INFO", msg); }
void Logger::warn(const std::string& msg) { log("WARN", msg); }
void Logger::error(const std::string& msg) { log("ERROR", msg); }

void Logger::log(const char* level, const std::string& msg) {
  std::lock_guard<std::mutex> lock(mu_);
  std::cout << "[" << now_string() << "] [" << level << "] " << msg << std::endl;
}

}  // namespace common
