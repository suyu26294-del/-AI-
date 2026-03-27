#pragma once

#include <atomic>
#include <string>

#include "common/socket_utils.hpp"

namespace device {

class DeviceClient {
 public:
  DeviceClient(std::string device_id, std::string host, int port);
  int run(int seconds = 30);

 private:
  std::string device_id_;
  std::string host_;
  int port_;
  std::atomic<bool> running_{true};
  common::TcpConnection conn_;
};

}  // namespace device
