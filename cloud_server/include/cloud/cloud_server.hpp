#pragma once

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "common/socket_utils.hpp"
#include "common/types.hpp"

namespace cloud {

struct DeviceInfo {
  std::string id;
  int64_t last_heartbeat_ms{0};
  std::shared_ptr<common::TcpConnection> conn;
};

struct NodeInfo {
  std::string id;
  bool online{true};
  int cpu{0};
  int current_tasks{0};
  int64_t last_heartbeat_ms{0};
  std::vector<common::ModelType> models;
  std::shared_ptr<common::TcpConnection> conn;
};

struct TaskRecord {
  std::string task_id;
  std::string device_id;
  common::TaskType type;
  std::string input_ref;
  int64_t submit_ts{0};
  int retries{0};
};

class CloudServer {
 public:
  explicit CloudServer(int port);
  void run();

 private:
  int port_;
  int server_fd_{-1};
  std::atomic<bool> running_{true};

  std::mutex device_mu_;
  std::unordered_map<std::string, DeviceInfo> devices_;

  std::mutex node_mu_;
  std::unordered_map<std::string, NodeInfo> nodes_;

  std::mutex queue_mu_;
  std::deque<TaskRecord> queue_;

  std::mutex metrics_mu_;
  uint64_t total_tasks_{0};
  uint64_t success_tasks_{0};
  uint64_t failed_tasks_{0};
  double total_latency_ms_{0};

  void accept_loop();
  void scheduler_loop();
  void monitor_loop();
  void cleanup_loop();
  void handle_connection(std::shared_ptr<common::TcpConnection> conn);
  std::optional<std::string> choose_node(const TaskRecord& task, common::ModelType model);
  common::ModelType choose_model(common::TaskType type);
  void mark_node_offline(const std::string& node_id);
};

}  // namespace cloud
