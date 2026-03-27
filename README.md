# 轻量级端边云 AI 任务调度与设备协同系统

本项目提供一个可直接运行的端-边-云协同调度实现：

- 云端调度服务（C++20）
- 边缘推理节点（C++20，支持 `INT8` / `FP16`）
- 设备客户端（可映射真实 STM32 协议网关）
- 多设备并发模拟器（用于压测和演示）
- 集成测试脚本（Python）

## 1. 架构与能力

- **设备侧**：注册、心跳、状态上报、任务提交。
- **云端**：设备/节点连接管理、任务队列、调度策略、结果回收、指标日志。
- **边缘侧**：节点注册、负载心跳、任务执行、模型感知模拟推理。
- **模拟测试**：一键启动云+2边缘+多设备并发。

## 2. 目录

```text
project_root/
├── CMakeLists.txt
├── common/
│   ├── include/common/
│   └── src/
├── cloud_server/
│   ├── include/cloud/
│   └── src/
├── edge_node/
│   ├── include/edge/
│   └── src/
├── device_client/
│   ├── include/device/
│   └── src/
├── simulator/
│   └── src/
├── scripts/
├── tests/
└── docs/
```

## 3. 协议

基于 TCP + JSON 行协议（每条消息一行 JSON）。

统一字段：

- `type`
- `source_id`
- `timestamp`
- `payload`

核心消息：

- `device_register`
- `device_heartbeat`
- `device_status`
- `task_submit`
- `node_register`
- `node_heartbeat`
- `task_dispatch`
- `task_result`

## 4. 调度策略

- **模型感知**：
  - `realtime` → 优先 `INT8`
  - `high_accuracy` → 优先 `FP16`
  - `normal` → 默认 `INT8`
- **低延迟优先**：按 `current_tasks * 100 + cpu` 选最小负载节点。
- **容错切换**：节点不可用时自动重试并改派，最多重试 3 次。

## 5. 快速运行

```bash
cmake -S . -B build
cmake --build build -j

# 启动云
./build/cloud_server 9000

# 启动两个边缘节点（另开终端）
./build/edge_node node-a 9000 fp16
./build/edge_node node-b 9000 int8

# 单设备
./build/device_client device-001 9000 20

# 或多设备并发模拟
./build/device_simulator 50 9000 20
```

一键演示：

```bash
./scripts/run_demo.sh 9000 20
```

## 6. 测试

```bash
ctest --test-dir build --output-on-failure
python3 tests/integration_test.py
```

## 7. 真实项目落地建议

- STM32 端推荐通过串口/以太网网关将数据转换为本项目 JSON 协议。
- 推理模块可将 `edge_node::execute_task` 替换为 ACL/TensorRT/ONNXRuntime 实际调用。
- 云端可将内存状态扩展到 SQLite/Redis/MySQL。
- 安全方面可新增设备鉴权、签名和 TLS。
