#pragma once

#include <mutex>
#include <string>

namespace common {

class Logger {
 public:
  static Logger& instance();
  void info(const std::string& msg);
  void warn(const std::string& msg);
  void error(const std::string& msg);

 private:
  void log(const char* level, const std::string& msg);
  std::mutex mu_;
};

}  // namespace common
