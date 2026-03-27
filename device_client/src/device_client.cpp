#include "device/device_client.hpp"

#include <chrono>
#include <thread>

#include "common/protocol.hpp"

namespace device {

DeviceClient::DeviceClient(std::string device_id, std::string host, int port)
    : device_id_(std::move(device_id)), host_(std::move(host)), port_(port) {}

int DeviceClient::run(int seconds) {
  conn_ = common::connect_to_server(host_, port_);
  if (!conn_.valid()) return 1;

  conn_.send_line(common::make_message("device_register", device_id_));

  const auto start = std::chrono::steady_clock::now();
  int seq = 0;
  while (running_) {
    conn_.send_line(common::make_message("device_heartbeat", device_id_));
    conn_.send_line(common::make_message("device_status", device_id_,
                                         {{"mode", "driving"},
                                          {"alarm", seq % 2 == 0 ? "1" : "0"},
                                          {"sensor", std::to_string(42 + (seq % 5))}}));
    if (seq % 3 == 0) {
      auto type = seq % 2 == 0 ? "realtime" : "high_accuracy";
      conn_.send_line(common::make_message("task_submit", device_id_,
                                           {{"task_id", device_id_ + "-task-" + std::to_string(seq)},
                                            {"task_type", type},
                                            {"input_ref", "mock://frame/" + std::to_string(seq)}}));
    }
    seq++;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (std::chrono::steady_clock::now() - start > std::chrono::seconds(seconds)) break;
  }

  return 0;
}

}  // namespace device
