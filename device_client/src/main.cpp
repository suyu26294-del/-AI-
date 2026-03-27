#include "device/device_client.hpp"

#include <cstring>
#include <iostream>

#include "common/protocol.hpp"

int main(int argc, char** argv) {
  if (argc > 1 && std::strcmp(argv[1], "--self-test") == 0) {
    auto raw = common::make_message("x", "id", {{"k", "1"}});
    auto decoded = common::decode_message(raw.substr(0, raw.size() - 1));
    if (!decoded.has_value() || common::payload_get(decoded->payload, "k") != "1") return 2;
    std::cout << "self-test pass\n";
    return 0;
  }

  std::string id = argc > 1 ? argv[1] : "device-001";
  int port = argc > 2 ? std::atoi(argv[2]) : 9000;
  int seconds = argc > 3 ? std::atoi(argv[3]) : 20;

  device::DeviceClient c(id, "127.0.0.1", port);
  return c.run(seconds);
}
