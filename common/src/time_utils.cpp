#include "common/time_utils.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace common {

int64_t unix_ms() {
  auto now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

std::string now_string() {
  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm tm = *std::localtime(&t);
  std::stringstream ss;
  ss << std::put_time(&tm, "%F %T");
  return ss.str();
}

}  // namespace common
