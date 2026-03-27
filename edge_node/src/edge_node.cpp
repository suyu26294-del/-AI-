#include "edge/edge_node.hpp"

#include <algorithm>
#include <chrono>
#include <random>

#include "common/logger.hpp"
#include "common/message.hpp"
#include "common/socket_utils.hpp"
#include "common/time_utils.hpp"

namespace edge {

EdgeNode::EdgeNode(std::string node_id, std::string host, int port, std::vector<std::string> models)
    : node_id_(std::move(node_id)), host_(std::move(host)), port_(port), models_(std::move(models)) {}

EdgeNode::~EdgeNode() { stop(); }

bool EdgeNode::start() {
  fd_ = common::connect_to_server(host_, port_);
  if (fd_ < 0) return false;

  std::string models_csv;
  for (size_t i = 0; i < models_.size(); ++i) {
    if (i) models_csv += ",";
    models_csv += models_[i];
  }

  common::Message reg{"node_register", node_id_, common::unix_ms(), {{"models", models_csv}}};
  if (!common::send_line(fd_, common::serialize(reg))) return false;

  running_ = true;
  recv_thread_ = std::thread(&EdgeNode::recv_loop, this);
  heartbeat_thread_ = std::thread(&EdgeNode::heartbeat_loop, this);
  return true;
}

void EdgeNode::stop() {
  if (!running_) return;
  running_ = false;
  common::close_fd(fd_);
  if (recv_thread_.joinable()) recv_thread_.join();
  if (heartbeat_thread_.joinable()) heartbeat_thread_.join();
}

void EdgeNode::recv_loop() {
  std::string line;
  while (running_ && common::recv_line(fd_, line)) {
    auto msg = common::parse_message(line);
    if (!msg || msg->type != "task_dispatch") continue;
    std::thread(&EdgeNode::execute_task, this,
                common::payload_get(msg->payload, "task_id", ""),
                common::payload_get(msg->payload, "model", "INT8"),
                common::payload_get(msg->payload, "input_ref", "none")).detach();
  }
}

void EdgeNode::heartbeat_loop() {
  std::mt19937 rng(std::random_device{}());
  std::uniform_real_distribution<double> jitter(-5.0, 5.0);
  while (running_) {
    double cpu = std::clamp(20.0 + current_tasks_.load() * 18.0 + jitter(rng), 0.0, 100.0);
    common::Message hb{"node_heartbeat", node_id_, common::unix_ms(),
                       {{"cpu_usage", std::to_string(cpu)},
                        {"current_tasks", std::to_string(current_tasks_.load())},
                        {"online", "true"}}};
    common::send_line(fd_, common::serialize(hb));
    std::this_thread::sleep_for(std::chrono::seconds(2));
  }
}

void EdgeNode::execute_task(const std::string& task_id, const std::string& model, const std::string& input_ref) {
  current_tasks_.fetch_add(1);
  auto start = std::chrono::steady_clock::now();
  std::mt19937 rng(std::random_device{}());
  std::uniform_int_distribution<int> dist(model == "INT8" ? 20 : 80, model == "INT8" ? 70 : 150);
  std::this_thread::sleep_for(std::chrono::milliseconds(dist(rng)));
  double exec_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();

  common::Message result{"task_result", node_id_, common::unix_ms(),
                         {{"task_id", task_id},
                          {"model", model},
                          {"exec_ms", std::to_string(exec_ms)},
                          {"success", "true"},
                          {"summary", "processed " + input_ref + " with model=" + model}}};
  common::send_line(fd_, common::serialize(result));
  current_tasks_.fetch_sub(1);
}

}  // namespace edge
