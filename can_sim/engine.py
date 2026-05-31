#!/usr/bin/env python3
"""
CAN 仿真引擎
通过 Unix Socket 向 can-dash 发送模拟 CAN 帧

Usage:
    python can_sim/engine.py
"""

import socket
import struct
import time
import random
import threading
from dataclasses import dataclass
from typing import Dict, List

SOCKET_PATH = "/tmp/can_dash_socket"


@dataclass
class CanFrame:
    can_id: int
    data: bytes
    timestamp_ms: int = 0


class CanSimulator:
    def __init__(self, socket_path: str = SOCKET_PATH):
        self.socket_path = socket_path
        self.running = False
        self.state = {
            "bat_volt": 350.0,
            "bat_curr": -50.0,
            "bat_soc": 75,
            "vehicle_speed": 0.0,
            "brake": 0,
            "motor_rpm": 0,
            "motor_temp": 45,
            # 安全带
            "driver_occupied": 1,
            "driver_buckled": 1,
            "passenger_occupied": 0,
            "passenger_buckled": 0,
            "rear_buckle": 0b00111,  # 后排3个座位都系了
        }

    def connect(self) -> socket.socket:
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.connect(self.socket_path)
        return sock

    def send_frame(self, sock: socket.socket, can_id: int, data: bytes):
        # Unix Socket 格式：[can_id(4bytes LE)][dlc(1byte)][data(variable)]
        msg = struct.pack("<IB", can_id, len(data)) + data
        sock.send(msg)

    def run(self):
        sock = self.connect()
        self.running = True

        print(f"[CanSim] Connected to {self.socket_path}")

        tick = 0
        while self.running:
            tick += 1
            now_ms = int(time.time() * 1000)

            # ─── BMS 帧 0x186040F3 (100ms) ───
            if tick % 10 == 0:
                # 模拟电压电流波动
                self.state["bat_volt"] += random.uniform(-0.5, 0.5)
                self.state["bat_volt"] = max(280, min(420, self.state["bat_volt"]))

                self.state["bat_curr"] += random.uniform(-2, 2)
                self.state["bat_curr"] = max(-500, min(500, self.state["bat_curr"]))

                # SOC 缓慢下降
                if tick % 100 == 0:
                    self.state["bat_soc"] = max(0, self.state["bat_soc"] - 1)

                bat_volt_raw = int(self.state["bat_volt"] * 10)
                bat_curr_raw = int(self.state["bat_curr"] * 10 + 10000)
                data = struct.pack("<HHb", bat_volt_raw, bat_curr_raw, self.state["bat_soc"])
                self.send_frame(sock, 0x186040F3, data)

            # ─── VCPU 帧 0x203 (50ms) ───
            if tick % 5 == 0:
                # 模拟加速/减速
                if self.state["vehicle_speed"] < 60:
                    self.state["vehicle_speed"] += 2.0
                else:
                    self.state["vehicle_speed"] = max(0, self.state["vehicle_speed"] - 5.0)

                self.state["brake"] = 50 if self.state["vehicle_speed"] > 80 else 0

                speed_raw = int(self.state["vehicle_speed"] * 10)
                data = struct.pack("<bHH", 0, self.state["brake"], speed_raw)
                self.send_frame(sock, 0x203, data)

            # ─── MCU 帧 0x101 (100ms) ───
            if tick % 10 == 5:
                self.state["motor_rpm"] = int(self.state["vehicle_speed"] * 50)
                self.state["motor_temp"] += random.uniform(-0.5, 1.0)
                self.state["motor_temp"] = max(30, min(150, self.state["motor_temp"]))

                motor_temp_raw = int(self.state["motor_temp"] + 40)
                data = struct.pack("<hhb", self.state["motor_rpm"], 0, motor_temp_raw)
                self.send_frame(sock, 0x101, data)

            # ─── 座椅占用帧 0x2F0/0x2F1 (500ms) ───
            if tick % 50 == 0:
                driver_occ = self.state["driver_occupied"] | (self.state["passenger_occupied"] << 1)
                self.send_frame(sock, 0x2F0, bytes([driver_occ]))
                self.send_frame(sock, 0x2F1, bytes([self.state["passenger_occupied"]]))

            # ─── 安全带帧 0x3B0/0x3B1/0x3B2 (200ms) ───
            if tick % 20 == 0:
                driver_buckle = self.state["driver_buckled"]
                passenger_buckle = self.state["passenger_buckled"] if self.state["passenger_occupied"] else 0
                self.send_frame(sock, 0x3B0, bytes([driver_buckle]))
                self.send_frame(sock, 0x3B1, bytes([passenger_buckle]))
                self.send_frame(sock, 0x3B2, bytes([self.state["rear_buckle"]]))

            time.sleep(0.05)  # 50ms 周期

        sock.close()
        print("[CanSim] Disconnected")


def main():
    sim = CanSimulator()
    reconnect_delay = 1.0
    max_delay = 5.0
    try:
        while True:
            try:
                sim.run()
            except (BrokenPipeError, ConnectionRefusedError, OSError) as e:
                print(f"[CanSim] Connection error: {e}, reconnecting in {reconnect_delay:.1f}s...")
                time.sleep(reconnect_delay)
                reconnect_delay = min(reconnect_delay * 1.5, max_delay)
                continue
            except KeyboardInterrupt:
                sim.running = False
                print("\n[CanSim] Stopped")
                break
    except KeyboardInterrupt:
        sim.running = False
        print("\n[CanSim] Stopped")


if __name__ == "__main__":
    main()
