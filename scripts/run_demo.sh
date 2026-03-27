#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" -j

cleanup() {
  kill "$SIM_PID" "$EDGE1_PID" "$EDGE2_PID" "$CLOUD_PID" >/dev/null 2>&1 || true
}
trap cleanup EXIT

"$BUILD_DIR/cloud_server" 9000 &
CLOUD_PID=$!
sleep 1
"$BUILD_DIR/edge_node" edge_a 127.0.0.1 9000 INT8,FP16 &
EDGE1_PID=$!
"$BUILD_DIR/edge_node" edge_b 127.0.0.1 9000 INT8 &
EDGE2_PID=$!
"$BUILD_DIR/multi_device_simulator" 127.0.0.1 9000 20 500 &
SIM_PID=$!

wait "$SIM_PID"
