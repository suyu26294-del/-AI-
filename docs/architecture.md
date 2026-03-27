# 轻量级端边云 AI 任务调度系统架构

## 核心模块

- **cloud_server**: 统一 TCP 接入、设备/节点状态管理、任务队列与调度、结果汇总、监控输出。
- **edge_node**: 节点注册、心跳上报、模型能力声明、任务执行（模拟推理）。
- **device_client**: STM32 真实设备可对接的最小协议实现。
- **multi_device_simulator**: 用于几十~数百设备并发压测。

## 协议

- 传输: TCP 行分隔 JSON（每条消息以 `\n` 结尾）。
- 通用字段: `type`, `source_id`, `timestamp`, `payload`。

## 调度策略

1. 按任务类型偏好模型：`realtime -> INT8`, `high_precision -> FP16`。
2. 在可用节点中按负载分值选择：`cpu_usage + current_tasks * 12`。
3. 节点不可用时自动回退到任意在线节点并记录切换成功率。
