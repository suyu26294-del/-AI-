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
  std::mutex mu_;
  void log(const std::string& level, const std::string& msg);
};

}  // namespace common
