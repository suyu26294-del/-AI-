#pragma once

#include <chrono>
#include <string>

namespace common {

enum class TaskType { Realtime, HighAccuracy, Normal };
enum class ModelType { INT8, FP16 };

inline std::string to_string(TaskType t) {
  switch (t) {
    case TaskType::Realtime: return "realtime";
    case TaskType::HighAccuracy: return "high_accuracy";
    case TaskType::Normal: return "normal";
  }
  return "normal";
}

inline std::string to_string(ModelType m) {
  switch (m) {
    case ModelType::INT8: return "INT8";
    case ModelType::FP16: return "FP16";
  }
  return "INT8";
}

inline TaskType parse_task_type(const std::string& v) {
  if (v == "realtime") return TaskType::Realtime;
  if (v == "high_accuracy") return TaskType::HighAccuracy;
  return TaskType::Normal;
}

inline ModelType parse_model_type(const std::string& v) {
  if (v == "FP16") return ModelType::FP16;
  return ModelType::INT8;
}

inline int64_t now_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

}  // namespace common
