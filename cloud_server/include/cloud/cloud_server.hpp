#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "common/message.hpp"
#include "common/types.hpp"

namespace cloud {

struct DeviceState {
  int fd{-1};
  common::TimePoint last_heartbeat{};
  std::string mode{"normal"};
  bool alert{false};
};

struct NodeState {
  int fd{-1};
  common::TimePoint last_heartbeat{};
  double cpu_usage{0.0};
  int current_tasks{0};
  std::unordered_set<std::string> models;
  bool online{false};
};

struct TaskRecord {
  std::string task_id;
  std::string device_id;
  common::TaskType task_type{common::TaskType::Normal};
  std::string input_ref;
  common::TimePoint enqueue_ts{};
  int retry_count{0};
};

struct Metrics {
  uint64_t total_tasks{0};
  uint64_t success_tasks{0};
  uint64_t failed_tasks{0};
  double avg_schedule_ms{0.0};
  double avg_exec_ms{0.0};
  uint64_t switch_attempts{0};
  uint64_t switch_success{0};
};

class CloudServer {
 public:
  explicit CloudServer(int port);
  ~CloudServer();

  bool start();
  void stop();

 private:
  void accept_loop();
  void session_loop(int client_fd);
  void dispatcher_loop();
  void monitor_loop();
  void print_metrics_loop();

  void handle_message(int fd, const common::Message& msg);
  void handle_device_register(int fd, const common::Message& msg);
  void handle_device_heartbeat(const common::Message& msg);
  void handle_device_status(const common::Message& msg);
  void handle_task_submit(const common::Message& msg);
  void handle_node_register(int fd, const common::Message& msg);
  void handle_node_heartbeat(const common::Message& msg);
  void handle_task_result(const common::Message& msg);

  std::optional<std::string> choose_node(const TaskRecord& task, common::ModelType& out_model, bool is_retry);
  void send_error(int fd, const std::string& reason);
  void mark_fd_offline(int fd);
  void update_avg(double sample, double& target, uint64_t count);

  int port_;
  int listen_fd_{-1};
  std::atomic<bool> running_{false};

  std::mutex mu_;
  std::unordered_map<int, std::string> fd_to_id_;
  std::unordered_map<int, common::Role> fd_to_role_;
  std::unordered_map<std::string, DeviceState> devices_;
  std::unordered_map<std::string, NodeState> nodes_;

  std::deque<TaskRecord> task_queue_;
  std::condition_variable queue_cv_;

  Metrics metrics_;

  std::thread accept_thread_;
  std::thread dispatch_thread_;
  std::thread monitor_thread_;
  std::thread metrics_thread_;
  std::vector<std::thread> session_threads_;
};

}  // namespace cloud
