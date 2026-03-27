#pragma once

#include <atomic>
#include <string>
#include <thread>

namespace device {

class DeviceClient {
 public:
  DeviceClient(std::string device_id, std::string host, int port, int submit_interval_ms = 3000);
  ~DeviceClient();

  bool start();
  void stop();

 private:
  void heartbeat_loop();
  void status_loop();
  void submit_task_loop();

  std::string next_task_id();

  std::string device_id_;
  std::string host_;
  int port_;
  int submit_interval_ms_;
  int fd_{-1};
  std::atomic<bool> running_{false};
  std::atomic<uint64_t> task_counter_{0};

  std::thread hb_thread_;
  std::thread status_thread_;
  std::thread task_thread_;
};

}  // namespace device
