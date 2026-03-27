#include "edge/edge_node.hpp"

#include <chrono>
#include <random>

#include "common/logger.hpp"
#include "common/protocol.hpp"
#include "common/types.hpp"

namespace edge {

using common::Logger;

EdgeNode::EdgeNode(std::string node_id, std::string host, int port, bool supports_fp16)
    : node_id_(std::move(node_id)), host_(std::move(host)), port_(port), supports_fp16_(supports_fp16) {}

void EdgeNode::run() {
  conn_ = common::connect_to_server(host_, port_);
  if (!conn_.valid()) {
    Logger::instance().error("node connect failed");
    return;
  }

  common::Payload p{{"models", supports_fp16_ ? "INT8|FP16" : "INT8"}};
  conn_.send_line(common::make_message("node_register", node_id_, p));

  std::thread hb([this] { heartbeat_loop(); });
  receive_loop();
  running_ = false;
  hb.join();
}

void EdgeNode::heartbeat_loop() {
  std::mt19937 rng(std::random_device{}());
  std::uniform_int_distribution<int> cpu_dist(15, 90);
  while (running_) {
    const int cpu = cpu_dist(rng);
    common::Payload p{{"cpu", std::to_string(cpu)}, {"current_tasks", std::to_string(current_tasks_.load())}};
    if (!conn_.send_line(common::make_message("node_heartbeat", node_id_, p))) {
      running_ = false;
      break;
    }
    std::this_thread::sleep_for(std::chrono::seconds(2));
  }
}

void EdgeNode::receive_loop() {
  while (running_ && conn_.valid()) {
    auto line = conn_.recv_line();
    if (!line.has_value()) break;
    auto msg = common::decode_message(*line);
    if (!msg.has_value() || msg->type != "task_dispatch") continue;

    const auto task_id = common::payload_get(msg->payload, "task_id");
    const auto device_id = common::payload_get(msg->payload, "device_id");
    const auto model = common::payload_get(msg->payload, "model", "INT8");
    const auto input_ref = common::payload_get(msg->payload, "input_ref");
    const auto submit_ts = std::stoll(common::payload_get(msg->payload, "submit_ts", "0"));

    pool_.enqueue([this, task_id, device_id, model, input_ref, submit_ts] {
      execute_task(task_id, device_id, model, input_ref, submit_ts);
    });
  }
}

void EdgeNode::execute_task(std::string task_id, std::string device_id, std::string model, std::string input_ref,
                            int64_t submit_ts) {
  (void)device_id;
  (void)input_ref;
  current_tasks_++;

  const int sleep_ms = model == "FP16" ? 130 : 55;
  std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));

  const auto now = common::now_ms();
  const double latency = static_cast<double>(now - submit_ts);
  common::Payload p{{"task_id", task_id},
                    {"model", model},
                    {"latency_ms", std::to_string(latency)},
                    {"result_summary", "mock_detection_ok"}};
  conn_.send_line(common::make_message("task_result", node_id_, p));
  current_tasks_--;
}

}  // namespace edge
