#pragma once

#include <atomic>
#include <string>
#include <thread>
#include <vector>

namespace edge {

class EdgeNode {
 public:
  EdgeNode(std::string node_id, std::string host, int port, std::vector<std::string> models);
  ~EdgeNode();

  bool start();
  void stop();

 private:
  void recv_loop();
  void heartbeat_loop();
  void execute_task(const std::string& task_id, const std::string& model, const std::string& input_ref);

  std::string node_id_;
  std::string host_;
  int port_;
  std::vector<std::string> models_;
  int fd_{-1};
  std::atomic<bool> running_{false};
  std::atomic<int> current_tasks_{0};

  std::thread recv_thread_;
  std::thread heartbeat_thread_;
};

}  // namespace edge
