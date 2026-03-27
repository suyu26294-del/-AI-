#!/usr/bin/env python3
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build"
PORT = "9100"


def run():
    BUILD.mkdir(exist_ok=True)
    subprocess.run(["cmake", "-S", str(ROOT), "-B", str(BUILD)], check=True)
    subprocess.run(["cmake", "--build", str(BUILD), "-j"], check=True)

    cloud_log = (BUILD / "itest_cloud.log").open("w")
    n1_log = (BUILD / "itest_node1.log").open("w")
    n2_log = (BUILD / "itest_node2.log").open("w")

    cloud = subprocess.Popen([str(BUILD / "cloud_server"), PORT], stdout=cloud_log, stderr=subprocess.STDOUT)
    node1 = subprocess.Popen([str(BUILD / "edge_node"), "node-1", PORT, "fp16"], stdout=n1_log, stderr=subprocess.STDOUT)
    node2 = subprocess.Popen([str(BUILD / "edge_node"), "node-2", PORT, "int8"], stdout=n2_log, stderr=subprocess.STDOUT)
    time.sleep(1)

    sim = subprocess.run([str(BUILD / "device_simulator"), "10", PORT, "8"], check=True)
    assert sim.returncode == 0

    time.sleep(2)
    for p in (node1, node2, cloud):
      p.terminate()
    for p in (node1, node2, cloud):
      p.wait(timeout=5)

    text = (BUILD / "itest_cloud.log").read_text()
    assert "dispatched task=" in text
    assert "task finished" in text


if __name__ == "__main__":
    run()
    print("integration test passed")
