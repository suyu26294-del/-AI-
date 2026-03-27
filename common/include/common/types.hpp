#pragma once

#include <chrono>
#include <string>

namespace common {

enum class Role { Device, Node, Unknown };
enum class TaskType { Realtime, HighPrecision, Normal };
enum class ModelType { Int8, Fp16 };

inline std::string to_string(TaskType t) {
  switch (t) {
    case TaskType::Realtime: return "realtime";
    case TaskType::HighPrecision: return "high_precision";
    case TaskType::Normal: return "normal";
  }
  return "normal";
}

inline std::string to_string(ModelType m) {
  return m == ModelType::Int8 ? "INT8" : "FP16";
}

inline TaskType task_type_from_string(const std::string& s) {
  if (s == "realtime") return TaskType::Realtime;
  if (s == "high_precision") return TaskType::HighPrecision;
  return TaskType::Normal;
}

inline ModelType model_type_from_string(const std::string& s) {
  return s == "INT8" ? ModelType::Int8 : ModelType::Fp16;
}

using Clock = std::chrono::steady_clock;
using TimePoint = std::chrono::time_point<Clock>;

}  // namespace common
