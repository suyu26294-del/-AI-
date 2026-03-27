#pragma once

#include <atomic>
#include <string>
#include <thread>

#include "common/socket_utils.hpp"
#include "common/thread_pool.hpp"

namespace edge {

class EdgeNode {
 public:
  EdgeNode(std::string node_id, std::string host, int port, bool supports_fp16);
  void run();

 private:
  std::string node_id_;
  std::string host_;
  int port_;
  bool supports_fp16_;
  std::atomic<bool> running_{true};
  std::atomic<int> current_tasks_{0};

  common::TcpConnection conn_;
  common::ThreadPool pool_{4};

  void heartbeat_loop();
  void receive_loop();
  void execute_task(std::string task_id, std::string device_id, std::string model, std::string input_ref,
                    int64_t submit_ts);
};

}  // namespace edge
