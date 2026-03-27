#include "cloud/cloud_server.hpp"

#include <algorithm>
#include <chrono>
#include <limits>
#include <sstream>

#include "common/logger.hpp"
#include "common/protocol.hpp"

namespace cloud {

using common::Logger;

CloudServer::CloudServer(int port) : port_(port) {}

void CloudServer::run() {
  server_fd_ = common::create_server_socket(port_);
  if (server_fd_ < 0) {
    Logger::instance().error("failed to create server socket");
    return;
  }
  Logger::instance().info("cloud server listening on port " + std::to_string(port_));

  std::thread accept_t([this] { accept_loop(); });
  std::thread sched_t([this] { scheduler_loop(); });
  std::thread monitor_t([this] { monitor_loop(); });
  std::thread cleanup_t([this] { cleanup_loop(); });

  accept_t.join();
  sched_t.join();
  monitor_t.join();
  cleanup_t.join();
}

void CloudServer::accept_loop() {
  while (running_) {
    auto conn = common::accept_client(server_fd_);
    if (!conn.valid()) continue;
    auto ptr = std::make_shared<common::TcpConnection>(std::move(conn));
    std::thread(&CloudServer::handle_connection, this, ptr).detach();
  }
}

common::ModelType CloudServer::choose_model(common::TaskType type) {
  if (type == common::TaskType::Realtime) return common::ModelType::INT8;
  if (type == common::TaskType::HighAccuracy) return common::ModelType::FP16;
  return common::ModelType::INT8;
}

std::optional<std::string> CloudServer::choose_node(const TaskRecord&, common::ModelType model) {
  std::lock_guard<std::mutex> lk(node_mu_);
  std::string best;
  int best_score = std::numeric_limits<int>::max();
  for (auto& [id, node] : nodes_) {
    if (!node.online || !node.conn || !node.conn->valid()) continue;
    if (std::find(node.models.begin(), node.models.end(), model) == node.models.end()) continue;
    const int score = node.current_tasks * 100 + node.cpu;
    if (score < best_score) {
      best_score = score;
      best = id;
    }
  }
  if (best.empty()) return std::nullopt;
  return best;
}

void CloudServer::scheduler_loop() {
  while (running_) {
    TaskRecord task;
    bool has_task = false;
    {
      std::lock_guard<std::mutex> lk(queue_mu_);
      if (!queue_.empty()) {
        task = queue_.front();
        queue_.pop_front();
        has_task = true;
      }
    }
    if (!has_task) {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      continue;
    }

    auto model = choose_model(task.type);
    auto node_id = choose_node(task, model);
    if (!node_id.has_value()) {
      if (task.retries < 3) {
        task.retries++;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::lock_guard<std::mutex> lk(queue_mu_);
        queue_.push_back(task);
      } else {
        std::lock_guard<std::mutex> lk(metrics_mu_);
        failed_tasks_++;
      }
      Logger::instance().warn("no available node for task " + task.task_id);
      continue;
    }

    std::shared_ptr<common::TcpConnection> conn;
    {
      std::lock_guard<std::mutex> lk(node_mu_);
      auto it = nodes_.find(*node_id);
      if (it != nodes_.end()) {
        conn = it->second.conn;
        it->second.current_tasks++;
      }
    }

    if (!conn || !conn->valid()) {
      mark_node_offline(*node_id);
      continue;
    }

    common::Payload p{{"task_id", task.task_id},
                      {"device_id", task.device_id},
                      {"task_type", common::to_string(task.type)},
                      {"input_ref", task.input_ref},
                      {"submit_ts", std::to_string(task.submit_ts)},
                      {"model", common::to_string(model)}};
    auto msg = common::make_message("task_dispatch", "cloud", p);
    if (!conn->send_line(msg)) {
      mark_node_offline(*node_id);
      task.retries++;
      std::lock_guard<std::mutex> lk(queue_mu_);
      queue_.push_back(task);
    } else {
      Logger::instance().info("dispatched task=" + task.task_id + " to node=" + *node_id + " model=" +
                              common::to_string(model));
    }
  }
}

void CloudServer::monitor_loop() {
  while (running_) {
    {
      std::lock_guard<std::mutex> lk(metrics_mu_);
      const double avg = success_tasks_ == 0 ? 0.0 : total_latency_ms_ / success_tasks_;
      Logger::instance().info("metrics total=" + std::to_string(total_tasks_) + " success=" +
                              std::to_string(success_tasks_) + " failed=" + std::to_string(failed_tasks_) +
                              " avg_latency_ms=" + std::to_string(avg));
    }
    std::this_thread::sleep_for(std::chrono::seconds(5));
  }
}

void CloudServer::cleanup_loop() {
  while (running_) {
    const auto now = common::now_ms();
    {
      std::lock_guard<std::mutex> lk(device_mu_);
      for (auto it = devices_.begin(); it != devices_.end();) {
        if (now - it->second.last_heartbeat_ms > 12000) {
          Logger::instance().warn("device offline: " + it->first);
          it = devices_.erase(it);
        } else {
          ++it;
        }
      }
    }
    {
      std::lock_guard<std::mutex> lk(node_mu_);
      for (auto& [id, node] : nodes_) {
        if (now - node.last_heartbeat_ms > 12000) node.online = false;
      }
    }
    std::this_thread::sleep_for(std::chrono::seconds(2));
  }
}

void CloudServer::mark_node_offline(const std::string& node_id) {
  std::lock_guard<std::mutex> lk(node_mu_);
  auto it = nodes_.find(node_id);
  if (it != nodes_.end()) {
    it->second.online = false;
    Logger::instance().warn("node offline: " + node_id);
  }
}

void CloudServer::handle_connection(std::shared_ptr<common::TcpConnection> conn) {
  std::string registered_id;
  std::string role;

  while (conn->valid()) {
    auto line = conn->recv_line();
    if (!line.has_value()) break;
    auto msg = common::decode_message(*line);
    if (!msg.has_value()) {
      Logger::instance().warn("invalid message");
      continue;
    }

    if (msg->type == "device_register") {
      role = "device";
      registered_id = msg->source_id;
      DeviceInfo d{msg->source_id, common::now_ms(), conn};
      std::lock_guard<std::mutex> lk(device_mu_);
      devices_[msg->source_id] = d;
      Logger::instance().info("device registered: " + msg->source_id);
    } else if (msg->type == "device_heartbeat" || msg->type == "device_status") {
      std::lock_guard<std::mutex> lk(device_mu_);
      auto it = devices_.find(msg->source_id);
      if (it != devices_.end()) it->second.last_heartbeat_ms = common::now_ms();
    } else if (msg->type == "task_submit") {
      TaskRecord task{common::payload_get(msg->payload, "task_id"),
                      msg->source_id,
                      common::parse_task_type(common::payload_get(msg->payload, "task_type", "normal")),
                      common::payload_get(msg->payload, "input_ref", "mock://default"), common::now_ms(), 0};
      {
        std::lock_guard<std::mutex> lk(queue_mu_);
        queue_.push_back(task);
      }
      {
        std::lock_guard<std::mutex> lk(metrics_mu_);
        total_tasks_++;
      }
      Logger::instance().info("task queued: " + task.task_id + " from=" + msg->source_id);
    } else if (msg->type == "node_register") {
      role = "node";
      registered_id = msg->source_id;
      NodeInfo node;
      node.id = msg->source_id;
      node.last_heartbeat_ms = common::now_ms();
      node.online = true;
      node.conn = conn;
      std::stringstream ss(common::payload_get(msg->payload, "models", "INT8"));
      std::string m;
      while (std::getline(ss, m, '|')) node.models.push_back(common::parse_model_type(m));
      {
        std::lock_guard<std::mutex> lk(node_mu_);
        nodes_[msg->source_id] = node;
      }
      Logger::instance().info("node registered: " + msg->source_id);
    } else if (msg->type == "node_heartbeat") {
      std::lock_guard<std::mutex> lk(node_mu_);
      auto it = nodes_.find(msg->source_id);
      if (it != nodes_.end()) {
        it->second.last_heartbeat_ms = common::now_ms();
        it->second.cpu = common::payload_get_int(msg->payload, "cpu", it->second.cpu);
        it->second.current_tasks = common::payload_get_int(msg->payload, "current_tasks", it->second.current_tasks);
        it->second.online = true;
      }
    } else if (msg->type == "task_result") {
      const std::string task_id = common::payload_get(msg->payload, "task_id");
      const std::string node_id = msg->source_id;
      const double latency = common::payload_get_double(msg->payload, "latency_ms", 0.0);
      {
        std::lock_guard<std::mutex> lk(metrics_mu_);
        success_tasks_++;
        total_latency_ms_ += latency;
      }
      {
        std::lock_guard<std::mutex> lk(node_mu_);
        auto it = nodes_.find(node_id);
        if (it != nodes_.end() && it->second.current_tasks > 0) it->second.current_tasks--;
      }
      Logger::instance().info("task finished task=" + task_id + " node=" + node_id + " latency=" +
                              std::to_string(latency));
    }
  }

  if (role == "node" && !registered_id.empty()) mark_node_offline(registered_id);
  if (role == "device" && !registered_id.empty()) {
    std::lock_guard<std::mutex> lk(device_mu_);
    devices_.erase(registered_id);
    Logger::instance().warn("device disconnected: " + registered_id);
  }
  conn->close();
}

}  // namespace cloud
