#include "cloud/cloud_server.hpp"

#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <limits>

#include "common/logger.hpp"
#include "common/socket_utils.hpp"
#include "common/time_utils.hpp"

namespace cloud {

namespace {
constexpr auto kDeviceTimeout = std::chrono::seconds(12);
constexpr auto kNodeTimeout = std::chrono::seconds(10);
}

CloudServer::CloudServer(int port) : port_(port) {}
CloudServer::~CloudServer() { stop(); }

bool CloudServer::start() {
  listen_fd_ = common::create_server_socket(port_);
  if (listen_fd_ < 0) {
    common::Logger::instance().error("failed to create cloud server socket");
    return false;
  }
  running_ = true;
  accept_thread_ = std::thread(&CloudServer::accept_loop, this);
  dispatch_thread_ = std::thread(&CloudServer::dispatcher_loop, this);
  monitor_thread_ = std::thread(&CloudServer::monitor_loop, this);
  metrics_thread_ = std::thread(&CloudServer::print_metrics_loop, this);
  common::Logger::instance().info("cloud server started on port " + std::to_string(port_));
  return true;
}

void CloudServer::stop() {
  if (!running_) return;
  running_ = false;
  queue_cv_.notify_all();
  common::close_fd(listen_fd_);
  if (accept_thread_.joinable()) accept_thread_.join();
  if (dispatch_thread_.joinable()) dispatch_thread_.join();
  if (monitor_thread_.joinable()) monitor_thread_.join();
  if (metrics_thread_.joinable()) metrics_thread_.join();
  for (auto& t : session_threads_) if (t.joinable()) t.join();
}

void CloudServer::accept_loop() {
  while (running_) {
    int client_fd = accept(listen_fd_, nullptr, nullptr);
    if (client_fd < 0) continue;
    session_threads_.emplace_back(&CloudServer::session_loop, this, client_fd);
  }
}

void CloudServer::session_loop(int client_fd) {
  std::string line;
  while (running_ && common::recv_line(client_fd, line)) {
    auto msg = common::parse_message(line);
    if (!msg.has_value()) {
      send_error(client_fd, "invalid json message");
      continue;
    }
    handle_message(client_fd, *msg);
  }
  mark_fd_offline(client_fd);
  common::close_fd(client_fd);
}

void CloudServer::handle_message(int fd, const common::Message& msg) {
  if (msg.type == "device_register") return handle_device_register(fd, msg);
  if (msg.type == "device_heartbeat") return handle_device_heartbeat(msg);
  if (msg.type == "device_status") return handle_device_status(msg);
  if (msg.type == "task_submit") return handle_task_submit(msg);
  if (msg.type == "node_register") return handle_node_register(fd, msg);
  if (msg.type == "node_heartbeat") return handle_node_heartbeat(msg);
  if (msg.type == "task_result") return handle_task_result(msg);
  if (msg.type == "metrics_request") {
    common::Message rsp;
    rsp.type = "metrics_snapshot";
    rsp.source_id = "cloud";
    rsp.timestamp_ms = common::unix_ms();
    {
      std::lock_guard<std::mutex> lock(mu_);
      rsp.payload = {
          {"total_tasks", std::to_string(metrics_.total_tasks)},
          {"success_tasks", std::to_string(metrics_.success_tasks)},
          {"failed_tasks", std::to_string(metrics_.failed_tasks)},
          {"avg_schedule_ms", std::to_string(metrics_.avg_schedule_ms)},
          {"avg_exec_ms", std::to_string(metrics_.avg_exec_ms)},
          {"online_devices", std::to_string(devices_.size())},
          {"online_nodes", std::to_string(std::count_if(nodes_.begin(), nodes_.end(), [](const auto& kv) { return kv.second.online; }))}};
    }
    common::send_line(fd, common::serialize(rsp));
    return;
  }
  send_error(fd, "unsupported message type: " + msg.type);
}

void CloudServer::handle_device_register(int fd, const common::Message& msg) {
  std::lock_guard<std::mutex> lock(mu_);
  devices_[msg.source_id] = DeviceState{fd, common::Clock::now()};
  fd_to_id_[fd] = msg.source_id;
  fd_to_role_[fd] = common::Role::Device;
  common::Logger::instance().info("device online: " + msg.source_id);
}

void CloudServer::handle_device_heartbeat(const common::Message& msg) {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = devices_.find(msg.source_id);
  if (it != devices_.end()) it->second.last_heartbeat = common::Clock::now();
}

void CloudServer::handle_device_status(const common::Message& msg) {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = devices_.find(msg.source_id);
  if (it == devices_.end()) return;
  it->second.last_heartbeat = common::Clock::now();
  it->second.mode = common::payload_get(msg.payload, "mode", "unknown");
  it->second.alert = common::payload_get_bool(msg.payload, "alert", false);
}

void CloudServer::handle_task_submit(const common::Message& msg) {
  TaskRecord task;
  task.task_id = common::payload_get(msg.payload, "task_id", "");
  task.device_id = msg.source_id;
  task.task_type = common::task_type_from_string(common::payload_get(msg.payload, "task_type", "normal"));
  task.input_ref = common::payload_get(msg.payload, "input_ref", "none");
  task.enqueue_ts = common::Clock::now();
  if (task.task_id.empty()) return;

  {
    std::lock_guard<std::mutex> lock(mu_);
    task_queue_.push_back(task);
    metrics_.total_tasks += 1;
  }
  queue_cv_.notify_one();
}

void CloudServer::handle_node_register(int fd, const common::Message& msg) {
  NodeState state;
  state.fd = fd;
  state.last_heartbeat = common::Clock::now();
  state.online = true;
  std::string models_csv = common::payload_get(msg.payload, "models", "INT8");
  size_t pos = 0;
  while ((pos = models_csv.find(',')) != std::string::npos) {
    state.models.insert(models_csv.substr(0, pos));
    models_csv.erase(0, pos + 1);
  }
  if (!models_csv.empty()) state.models.insert(models_csv);

  std::lock_guard<std::mutex> lock(mu_);
  nodes_[msg.source_id] = std::move(state);
  fd_to_id_[fd] = msg.source_id;
  fd_to_role_[fd] = common::Role::Node;
  common::Logger::instance().info("node online: " + msg.source_id);
}

void CloudServer::handle_node_heartbeat(const common::Message& msg) {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = nodes_.find(msg.source_id);
  if (it == nodes_.end()) return;
  it->second.last_heartbeat = common::Clock::now();
  it->second.cpu_usage = common::payload_get_double(msg.payload, "cpu_usage", 0.0);
  it->second.current_tasks = common::payload_get_int(msg.payload, "current_tasks", 0);
  it->second.online = true;
}

void CloudServer::handle_task_result(const common::Message& msg) {
  const auto task_id = common::payload_get(msg.payload, "task_id", "");
  const auto node_id = msg.source_id;
  const auto exec_ms = common::payload_get_double(msg.payload, "exec_ms", 0.0);

  std::lock_guard<std::mutex> lock(mu_);
  if (common::payload_get_bool(msg.payload, "success", true)) metrics_.success_tasks += 1;
  else metrics_.failed_tasks += 1;

  update_avg(exec_ms, metrics_.avg_exec_ms, metrics_.success_tasks + metrics_.failed_tasks);
  auto it = nodes_.find(node_id);
  if (it != nodes_.end() && it->second.current_tasks > 0) it->second.current_tasks--;
  common::Logger::instance().info("task result: task_id=" + task_id + " node=" + node_id);
}

std::optional<std::string> CloudServer::choose_node(const TaskRecord& task, common::ModelType& out_model, bool is_retry) {
  std::lock_guard<std::mutex> lock(mu_);
  const std::string preferred_model = (task.task_type == common::TaskType::HighPrecision) ? "FP16" : "INT8";

  std::string best_node;
  double best_score = std::numeric_limits<double>::max();
  for (const auto& [id, n] : nodes_) {
    if (!n.online || !n.models.contains(preferred_model)) continue;
    const double score = n.cpu_usage + n.current_tasks * 12.0;
    if (score < best_score) { best_score = score; best_node = id; }
  }

  if (best_node.empty()) {
    for (const auto& [id, n] : nodes_) {
      if (!n.online) continue;
      const double score = n.cpu_usage + n.current_tasks * 12.0;
      if (score < best_score) { best_score = score; best_node = id; }
    }
    if (is_retry) {
      metrics_.switch_attempts++;
      if (!best_node.empty()) metrics_.switch_success++;
    }
  }

  if (best_node.empty()) return std::nullopt;
  auto& node = nodes_[best_node];
  out_model = node.models.contains(preferred_model) ? common::model_type_from_string(preferred_model)
                                                     : (node.models.contains("INT8") ? common::ModelType::Int8 : common::ModelType::Fp16);
  node.current_tasks++;
  return best_node;
}

void CloudServer::dispatcher_loop() {
  while (running_) {
    TaskRecord task;
    {
      std::unique_lock<std::mutex> lock(mu_);
      queue_cv_.wait_for(lock, std::chrono::milliseconds(500), [this] { return !task_queue_.empty() || !running_; });
      if (!running_) break;
      if (task_queue_.empty()) continue;
      task = task_queue_.front();
      task_queue_.pop_front();
    }

    common::ModelType model = common::ModelType::Int8;
    auto node_id_opt = choose_node(task, model, task.retry_count > 0);
    if (!node_id_opt) {
      if (task.retry_count < 2) {
        task.retry_count++;
        std::lock_guard<std::mutex> lock(mu_);
        task_queue_.push_back(task);
      } else {
        std::lock_guard<std::mutex> lock(mu_);
        metrics_.failed_tasks++;
      }
      continue;
    }

    int node_fd;
    {
      std::lock_guard<std::mutex> lock(mu_);
      node_fd = nodes_[*node_id_opt].fd;
      double schedule_ms = std::chrono::duration<double, std::milli>(common::Clock::now() - task.enqueue_ts).count();
      update_avg(schedule_ms, metrics_.avg_schedule_ms, metrics_.total_tasks);
    }

    common::Message dispatch;
    dispatch.type = "task_dispatch";
    dispatch.source_id = "cloud";
    dispatch.timestamp_ms = common::unix_ms();
    dispatch.payload = {{"task_id", task.task_id},
                        {"device_id", task.device_id},
                        {"task_type", common::to_string(task.task_type)},
                        {"input_ref", task.input_ref},
                        {"model", common::to_string(model)}};

    if (!common::send_line(node_fd, common::serialize(dispatch))) {
      std::lock_guard<std::mutex> lock(mu_);
      nodes_[*node_id_opt].online = false;
      nodes_[*node_id_opt].current_tasks = std::max(0, nodes_[*node_id_opt].current_tasks - 1);
      task.retry_count++;
      task_queue_.push_front(task);
      continue;
    }
    common::Logger::instance().info("task dispatched: " + task.task_id + " -> " + *node_id_opt);
  }
}

void CloudServer::monitor_loop() {
  while (running_) {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    auto now = common::Clock::now();
    std::lock_guard<std::mutex> lock(mu_);
    for (auto it = devices_.begin(); it != devices_.end();) {
      if (now - it->second.last_heartbeat > kDeviceTimeout) it = devices_.erase(it); else ++it;
    }
    for (auto& [id, node] : nodes_) {
      if (now - node.last_heartbeat > kNodeTimeout) node.online = false;
    }
  }
}

void CloudServer::print_metrics_loop() {
  while (running_) {
    std::this_thread::sleep_for(std::chrono::seconds(5));
    std::lock_guard<std::mutex> lock(mu_);
    size_t online_nodes = std::count_if(nodes_.begin(), nodes_.end(), [](const auto& kv) { return kv.second.online; });
    common::Logger::instance().info("metrics: devices=" + std::to_string(devices_.size()) + " nodes=" + std::to_string(online_nodes) +
      " total=" + std::to_string(metrics_.total_tasks) + " ok=" + std::to_string(metrics_.success_tasks));
  }
}

void CloudServer::send_error(int fd, const std::string& reason) {
  common::Message err;
  err.type = "error";
  err.source_id = "cloud";
  err.timestamp_ms = common::unix_ms();
  err.payload = {{"reason", reason}};
  common::send_line(fd, common::serialize(err));
}

void CloudServer::mark_fd_offline(int fd) {
  std::lock_guard<std::mutex> lock(mu_);
  auto r = fd_to_role_.find(fd);
  auto i = fd_to_id_.find(fd);
  if (r == fd_to_role_.end() || i == fd_to_id_.end()) return;
  if (r->second == common::Role::Device) devices_.erase(i->second);
  if (r->second == common::Role::Node && nodes_.contains(i->second)) nodes_[i->second].online = false;
  fd_to_role_.erase(r);
  fd_to_id_.erase(i);
}

void CloudServer::update_avg(double sample, double& target, uint64_t count) {
  if (count == 0) return;
  target += (sample - target) / static_cast<double>(count);
}

}  // namespace cloud
