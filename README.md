# 轻量级端边云 AI 任务调度与设备协同系统

本仓库提供一个可直接运行的端边云协同示例：
- 云端调度服务（C++20）
- 边缘节点执行服务（支持 INT8/FP16 模型模式）
- 设备客户端（可映射 STM32 通信逻辑）
- 多设备并发模拟器
- 集成测试（多节点 + 多设备）

## 1. 项目结构

```text
.
├── CMakeLists.txt
├── common/
├── cloud_server/
├── edge_node/
├── device_client/
├── simulator/
├── tests/
├── scripts/
├── config/
└── docs/
```

## 2. 快速开始

```bash
cmake -S . -B build
cmake --build build -j
```

### 启动云服务
```bash
./build/cloud_server 9000
```

### 启动边缘节点（两个）
```bash
./build/edge_node edge_a 127.0.0.1 9000 INT8,FP16
./build/edge_node edge_b 127.0.0.1 9000 INT8
```

### 启动单设备客户端
```bash
./build/device_client stm32_001 127.0.0.1 9000 3000
```

### 启动多设备模拟（例如 100 台）
```bash
./build/multi_device_simulator 127.0.0.1 9000 100 500
```

## 3. 通信协议

统一 JSON：
```json
{
  "type": "task_submit",
  "source_id": "stm32_001",
  "timestamp": 1710000000000,
  "payload": {
    "task_id": "stm32_001_task_1",
    "task_type": "realtime",
    "input_ref": "camera_frame_1"
  }
}
```

支持消息类型：
- device_register
- device_heartbeat
- device_status
- task_submit
- node_register
- node_heartbeat
- task_dispatch
- task_result
- metrics_request / metrics_snapshot

## 4. 调度与容错

- 实时任务优先 INT8，高精度任务优先 FP16。
- 优先选择负载分值低的节点（CPU + 并发任务数加权）。
- 节点离线/发送失败时自动重试与改派。

## 5. 集成测试

```bash
ctest --test-dir build --output-on-failure
```

测试会自动拉起：
- 1 个 cloud_server
- 2 个 edge_node
- 8 个模拟设备
- 10 秒后请求 metrics_snapshot 并断言关键指标

## 6. 演示脚本

```bash
./scripts/run_demo.sh
```

## 7. 对接真实 STM32 的建议

1. 保持同样 JSON 消息结构与心跳机制。
2. 串口/以太网模块侧封装 TCP + 行分隔协议。
3. 若 MCU 资源受限，可将 JSON 替换为 CBOR/Protobuf，云端保持适配层。
4. 建议增加设备鉴权签名（HMAC）与重放保护。

## 8. 后续可扩展

- 接入真实推理引擎（TensorRT/ACL）
- 持久化（SQLite / Redis / MySQL）
- 控制台升级为 Qt/Web UI
- 调度策略升级（优先级队列、SLA、预测负载）
