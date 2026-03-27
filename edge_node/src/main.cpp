#include <csignal>
#include <iostream>
#include <thread>

#include "edge/edge_node.hpp"

namespace {
volatile std::sig_atomic_t g_stop = 0;
void handle_sigint(int) { g_stop = 1; }
}

int main(int argc, char** argv) {
  if (argc < 4) {
    std::cerr << "Usage: edge_node <node_id> <cloud_host> <cloud_port> [INT8,FP16]\n";
    return 1;
  }

  std::string node_id = argv[1];
  std::string host = argv[2];
  int port = std::stoi(argv[3]);
  std::vector<std::string> models{"INT8", "FP16"};
  if (argc >= 5) {
    models.clear();
    std::string csv = argv[4];
    size_t pos = 0;
    while ((pos = csv.find(',')) != std::string::npos) {
      models.push_back(csv.substr(0, pos));
      csv.erase(0, pos + 1);
    }
    if (!csv.empty()) models.push_back(csv);
  }

  std::signal(SIGINT, handle_sigint);

  edge::EdgeNode node(node_id, host, port, models);
  if (!node.start()) {
    std::cerr << "edge node failed to start\n";
    return 1;
  }

  while (!g_stop) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  node.stop();
  return 0;
}
