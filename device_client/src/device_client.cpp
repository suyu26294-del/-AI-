#include "device/device_client.hpp"

#include <chrono>
#include <random>

#include "common/message.hpp"
#include "common/socket_utils.hpp"
#include "common/time_utils.hpp"

namespace device {

DeviceClient::DeviceClient(std::string device_id, std::string host, int port, int submit_interval_ms)
    : device_id_(std::move(device_id)), host_(std::move(host)), port_(port), submit_interval_ms_(submit_interval_ms) {}

DeviceClient::~DeviceClient() { stop(); }

bool DeviceClient::start() {
  fd_ = common::connect_to_server(host_, port_);
  if (fd_ < 0) return false;
  common::Message reg{"device_register", device_id_, common::unix_ms(), {{"hw", "STM32-sim"}, {"fw", "1.0.0"}}};
  if (!common::send_line(fd_, common::serialize(reg))) return false;

  running_ = true;
  hb_thread_ = std::thread(&DeviceClient::heartbeat_loop, this);
  status_thread_ = std::thread(&DeviceClient::status_loop, this);
  task_thread_ = std::thread(&DeviceClient::submit_task_loop, this);
  return true;
}

void DeviceClient::stop() {
  if (!running_) return;
  running_ = false;
  common::close_fd(fd_);
  if (hb_thread_.joinable()) hb_thread_.join();
  if (status_thread_.joinable()) status_thread_.join();
  if (task_thread_.joinable()) task_thread_.join();
}

void DeviceClient::heartbeat_loop() {
  while (running_) {
    common::Message hb{"device_heartbeat", device_id_, common::unix_ms(), {{"alive", "true"}}};
    common::send_line(fd_, common::serialize(hb));
    std::this_thread::sleep_for(std::chrono::seconds(3));
  }
}

void DeviceClient::status_loop() {
  std::mt19937 rng(std::random_device{}());
  std::uniform_int_distribution<int> sensor(0, 100);
  while (running_) {
    int s = sensor(rng);
    bool alert = s > 90;
    common::Message st{"device_status", device_id_, common::unix_ms(),
                       {{"mode", alert ? "warning" : "normal"}, {"alert", alert ? "true" : "false"}, {"sensor", std::to_string(s)}}};
    common::send_line(fd_, common::serialize(st));
    std::this_thread::sleep_for(std::chrono::seconds(4));
  }
}

void DeviceClient::submit_task_loop() {
  std::mt19937 rng(std::random_device{}());
  std::uniform_int_distribution<int> type_dist(0, 2);
  while (running_) {
    std::string task_type = "normal";
    int t = type_dist(rng);
    if (t == 0) task_type = "realtime";
    if (t == 1) task_type = "high_precision";
    common::Message submit{"task_submit", device_id_, common::unix_ms(),
                           {{"task_id", next_task_id()}, {"task_type", task_type}, {"input_ref", "camera_" + std::to_string(common::unix_ms())}}};
    common::send_line(fd_, common::serialize(submit));
    std::this_thread::sleep_for(std::chrono::milliseconds(submit_interval_ms_));
  }
}

std::string DeviceClient::next_task_id() {
  return device_id_ + "_task_" + std::to_string(task_counter_.fetch_add(1));
}

}  // namespace device
