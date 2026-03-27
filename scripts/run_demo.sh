#!/usr/bin/env bash
set -euo pipefail

PORT=${1:-9000}
DURATION=${2:-20}

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
BUILD_DIR="$ROOT_DIR/build"
mkdir -p "$BUILD_DIR"
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" >/dev/null
cmake --build "$BUILD_DIR" -j >/dev/null

"$BUILD_DIR/cloud_server" "$PORT" >"$BUILD_DIR/cloud.log" 2>&1 &
CLOUD_PID=$!
"$BUILD_DIR/edge_node" node-a "$PORT" fp16 >"$BUILD_DIR/node-a.log" 2>&1 &
NODE_A_PID=$!
"$BUILD_DIR/edge_node" node-b "$PORT" int8 >"$BUILD_DIR/node-b.log" 2>&1 &
NODE_B_PID=$!

sleep 1
"$BUILD_DIR/device_simulator" 30 "$PORT" "$DURATION" >"$BUILD_DIR/sim.log" 2>&1 || true

kill "$NODE_A_PID" "$NODE_B_PID" "$CLOUD_PID" 2>/dev/null || true
wait "$NODE_A_PID" "$NODE_B_PID" "$CLOUD_PID" 2>/dev/null || true

echo "logs: $BUILD_DIR/*.log"
