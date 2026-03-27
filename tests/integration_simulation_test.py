#!/usr/bin/env python3
import json
import os
import signal
import socket
import subprocess
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"


def send_msg(sock: socket.socket, msg: dict):
    sock.sendall((json.dumps(msg) + "\n").encode())


def recv_line(sock: socket.socket, timeout: float = 2.0):
    sock.settimeout(timeout)
    data = b""
    while not data.endswith(b"\n"):
        chunk = sock.recv(1)
        if not chunk:
            return None
        data += chunk
    return json.loads(data.decode().strip())


def main():
    cloud = subprocess.Popen([str(BUILD / "cloud_server"), "9001"])
    edge1 = subprocess.Popen([str(BUILD / "edge_node"), "edge_int8", "127.0.0.1", "9001", "INT8"])
    edge2 = subprocess.Popen([str(BUILD / "edge_node"), "edge_fp16", "127.0.0.1", "9001", "FP16,INT8"])
    sim = subprocess.Popen([str(BUILD / "multi_device_simulator"), "127.0.0.1", "9001", "8", "400"])

    try:
      time.sleep(10)
      with socket.create_connection(("127.0.0.1", 9001), timeout=3) as sock:
          req = {
              "type": "metrics_request",
              "source_id": "test_client",
              "timestamp": int(time.time() * 1000),
              "payload": {}
          }
          send_msg(sock, req)
          rsp = recv_line(sock)
          assert rsp and rsp.get("type") == "metrics_snapshot", f"bad response: {rsp}"
          payload = rsp.get("payload", {})
          assert int(payload.get("total_tasks", "0")) > 5, payload
          assert int(payload.get("success_tasks", "0")) > 0, payload
          assert int(payload.get("online_nodes", "0")) >= 1, payload
          print("integration test passed", payload)
    finally:
      for p in [sim, edge2, edge1, cloud]:
          p.send_signal(signal.SIGINT)
      for p in [sim, edge2, edge1, cloud]:
          try:
              p.wait(timeout=5)
          except subprocess.TimeoutExpired:
              p.kill()


if __name__ == "__main__":
    main()
