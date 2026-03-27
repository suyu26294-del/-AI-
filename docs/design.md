# Design Notes

- Cloud server: accept loop + scheduler loop + monitor loop + cleanup loop.
- Task queue: in-memory deque, FIFO.
- Node health: heartbeat timeout 12s.
- Metrics: total/success/failure/avg-latency.

This baseline is suitable for course/demo usage and can be evolved toward production.
