#!/usr/bin/env python3
"""
CAN 仿真引擎
通过 Unix Socket 向 can-dash 发送模拟 CAN 帧

Usage:
    python can_sim/engine.py [--scenario driving|charging|fault|idle|highway|low_battery] [--duration 30]

Scenarios:
    driving      - 正常驾驶循环（默认）：加速 → 巡航 → 减速
    charging     - 充电场景：bat_curr 负、charge_status=1、vehicle_speed=0
    fault        - 故障注入：触发电机超温 + 发动机故障 + 低胎压
    idle         - 静止：所有信号保持初始值
    highway      - 高速巡航：持续 120-130km/h, energy_mode=2 (engine boost), 高功耗
    low_battery  - 低电量：SOC<10% 触发 bat_soc_low + soc_critical_low 报警, 跛行降速
"""

import argparse
import socket
import struct
import time
import random
import math
from dataclasses import dataclass, field
from typing import Dict, Optional

SOCKET_PATH = "/tmp/can_processor_socket"


@dataclass
class Scenario:
    """仿真场景参数"""
    name: str = "driving"
    # 信号初始值
    initial: Dict[str, float] = field(default_factory=lambda: {
        "bat_volt": 350.0, "bat_curr": -50.0, "bat_soc": 75,
        "vehicle_speed": 0.0, "brake": 0, "motor_rpm": 0, "motor_temp": 45,
        "engine_rpm": 0, "engine_fault": 0,
        "charge_status": 0, "charge_fault": 0, "charge_power": 0,
        "energy_mode": 0, "ev_range": 80, "fuel_level": 60, "fuel_range": 500,
        "battery_temp": 30, "gear_status": 0,
        "tire_pressure_fl": 2.3, "tire_pressure_fr": 2.3,
        "tire_pressure_rl": 2.3, "tire_pressure_rr": 2.3,
        "driver_occupied": 1, "driver_buckled": 1,
        "passenger_occupied": 0, "passenger_buckled": 0,
        "rear_buckle": 0b00111,
    })
    # tick 回调（每个 tick 调用，传入当前状态和 tick 序号）
    tick_callback: Optional[callable] = None


def make_scenario(name: str) -> Scenario:
    """工厂方法：根据名字构造场景"""
    s = Scenario(name=name)
    if name == "driving":
        s.initial["vehicle_speed"] = 0
        # 默认场景无 callback，使用状态机
    elif name == "charging":
        s.initial.update({
            "vehicle_speed": 0, "brake": 0, "motor_rpm": 0, "motor_temp": 35,
            "bat_curr": -150.0, "bat_soc": 45, "charge_status": 1,
            "charge_power": 22, "energy_mode": 3,  # 3 = charge mode
            "engine_rpm": 0, "engine_fault": 0,
        })
    elif name == "fault":
        s.initial.update({
            "motor_temp": 130,        # 触发 motor_overtemp
            "engine_fault": 1,        # 触发 engine_fault_alarm
            "tire_pressure_fl": 1.6,  # 触发 tire_pressure_low
            "bat_volt": 425,          # 触发 bat_overvolt
        })
    elif name == "idle":
        # 全部保持初始值
        pass
    elif name == "highway":
        # 高速巡航：起始 SOC 低一些 (40%), 模拟长途行驶
        s.initial.update({
            "vehicle_speed": 125,
            "bat_soc": 40,  # 长途行驶
            "energy_mode": 2,  # engine boost
            "brake": 0,
        })
    elif name == "low_battery":
        # 低电量场景：SOC < 10% 触发 bat_soc_low 和 soc_critical_low 报警
        # engine_rpm=0 模拟跛行降速 (limp home)
        s.initial.update({
            "vehicle_speed": 0,  # 跛行模式先静止
            "bat_soc": 6,        # < 5% 触发 soc_critical_low, < 10% 触发 bat_soc_low
            "bat_volt": 320,     # 低 SOC 对应低电压
            "energy_mode": 0,    # EV 模式
            "brake": 1,          # 跛行时刹车
            "engine_rpm": 0,
        })
    return s


@dataclass
class CanFrame:
    can_id: int
    data: bytes
    timestamp_ms: int = 0


class CanSimulator:
    def __init__(self, socket_path: str = SOCKET_PATH, scenario_name: str = "driving"):
        self.socket_path = socket_path
        self.running = False
        self.scenario = make_scenario(scenario_name)
        self.state = dict(self.scenario.initial)

    def connect(self) -> socket.socket:
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.connect(self.socket_path)
        return sock

    def send_frame(self, sock: socket.socket, can_id: int, data: bytes):
        # Unix Socket 格式：[can_id(4bytes LE)][dlc(1byte)][data(variable)]
        msg = struct.pack("<IB", can_id, len(data)) + data
        # sendall 替代 send 避免 partial send 错位 (CAN frame 长度固定, 一次 send 可能
        # 只发出部分, 后续帧拼接在 rx_buf_ 里就乱了. sendall 阻塞到完整发送.)
        sock.sendall(msg)

    def update_driving(self, tick: int):
        """驾驶循环：每 12s 一个完整周期（0→120km/h→0），让 60s sparkline 装 5 个完整波"""
        # 周期 240 ticks（12s @ 50ms）
        phase = (tick % 240) / 240.0
        # 三角波：0→120→0
        if phase < 0.4:
            # 加速 0→120
            self.state["vehicle_speed"] = (phase / 0.4) * 120
        elif phase < 0.7:
            # 巡航 120
            self.state["vehicle_speed"] = 120
        else:
            # 减速 120→0
            self.state["vehicle_speed"] = (1.0 - (phase - 0.7) / 0.3) * 120
        self.state["vehicle_speed"] += random.uniform(-5, 5)  # 加大抖动让曲线有"呼吸"
        self.state["vehicle_speed"] = max(0, min(260, self.state["vehicle_speed"]))

        # 制动
        self.state["brake"] = 1 if self.state["vehicle_speed"] > 100 else 0

        # 电机
        self.state["motor_rpm"] = int(self.state["vehicle_speed"] * 50)
        self.state["motor_temp"] = 45 + (self.state["vehicle_speed"] / 120) * 50
        self.state["motor_temp"] += random.uniform(-1, 1)
        self.state["motor_temp"] = max(30, min(150, self.state["motor_temp"]))

        # 电池
        self.state["bat_volt"] += random.uniform(-0.3, 0.3)
        self.state["bat_volt"] = max(330, min(370, self.state["bat_volt"]))
        self.state["bat_curr"] = -50 + (self.state["vehicle_speed"] / 120) * -100
        self.state["bat_curr"] += random.uniform(-5, 5)

        # SOC 缓慢下降
        if tick % 200 == 0:
            self.state["bat_soc"] = max(0, self.state["bat_soc"] - 1)

        # 能量模式
        if self.state["vehicle_speed"] < 5 and self.state["bat_soc"] > 20:
            self.state["energy_mode"] = 0  # EV
        elif self.state["vehicle_speed"] < 30:
            self.state["energy_mode"] = 1  # Hybrid
        else:
            self.state["energy_mode"] = 2  # Engine boost

        # 燃油 + 续航
        if self.state["energy_mode"] == 2:
            self.state["fuel_level"] = max(0, self.state["fuel_level"] - 0.001)
            self.state["engine_rpm"] = int(self.state["vehicle_speed"] * 30)

        self.state["ev_range"] = max(0, int(self.state["bat_soc"] * 1.0))
        self.state["fuel_range"] = int(self.state["fuel_level"] * 8)

    def update_charging(self, tick: int):
        """充电场景：bat_curr 负，SOC 上升"""
        # 充电 1% / 10s
        if tick % 200 == 0:
            self.state["bat_soc"] = min(100, self.state["bat_soc"] + 1)
        # 电压随 SOC 上升
        self.state["bat_volt"] = 350 + (self.state["bat_soc"] - 50) * 0.5
        self.state["bat_curr"] = -150 + random.uniform(-10, 10)
        self.state["bat_volt"] += random.uniform(-0.5, 0.5)
        # 充电功率
        self.state["charge_power"] = abs(self.state["bat_volt"] * self.state["bat_curr"] / 1000)
        self.state["charge_power"] += random.uniform(-0.5, 0.5)
        # 充电时温度略升
        self.state["battery_temp"] = 32 + (self.state["bat_soc"] / 100) * 5
        self.state["motor_temp"] = 30  # 电机不动
        self.state["motor_rpm"] = 0
        self.state["vehicle_speed"] = 0
        self.state["brake"] = 0
        self.state["charge_status"] = 1
        self.state["charge_fault"] = 0
        self.state["energy_mode"] = 3  # charge

    def update_fault(self, tick: int):
        """故障注入：触发 3 个报警"""
        # motor_overtemp（threshold 120）
        self.state["motor_temp"] = 130 + math.sin(tick * 0.05) * 3
        # engine_fault_alarm（threshold ==1）
        self.state["engine_fault"] = 1
        # tire_pressure_low（threshold <1.8）
        self.state["tire_pressure_fl"] = 1.6
        # bat_overvolt（threshold >420）
        self.state["bat_volt"] = 425
        # 其它信号正常
        self.state["vehicle_speed"] = 60 + math.sin(tick * 0.1) * 5
        self.state["motor_rpm"] = int(self.state["vehicle_speed"] * 50)
        self.state["engine_rpm"] = int(self.state["vehicle_speed"] * 30)
        self.state["charge_status"] = 0
        self.state["energy_mode"] = 1

    def update_idle(self, tick: int):
        """静止：所有信号保持"""
        pass  # self.state 已经初始化，不动

    def update_highway(self, tick: int):
        """高速巡航：维持 120-130km/h, 高能耗, SOC 持续下降
        周期 600 ticks（30s @ 50ms），长周期保证 SOC 慢慢下降
        模拟长途高速: 持续 engine boost, 高 motor_rpm, 高 bat_curr (放电)
        """
        # 长周期小幅波动（120-130）
        phase = (tick % 600) / 600.0
        # 慢速正弦波动
        self.state["vehicle_speed"] = 125 + math.sin(phase * 2 * math.pi) * 5
        self.state["vehicle_speed"] += random.uniform(-1, 1)  # 小幅抖动
        self.state["vehicle_speed"] = max(0, min(260, self.state["vehicle_speed"]))

        # 不踩刹车（巡航）
        self.state["brake"] = 0

        # 电机高转速、高温
        self.state["motor_rpm"] = int(self.state["vehicle_speed"] * 50)
        self.state["motor_temp"] = 75 + (self.state["vehicle_speed"] / 130) * 30
        self.state["motor_temp"] += random.uniform(-0.5, 0.5)
        self.state["motor_temp"] = max(30, min(150, self.state["motor_temp"]))

        # 电池高放电（bat_curr 负数 = 放电）
        self.state["bat_volt"] += random.uniform(-0.3, 0.3)
        self.state["bat_volt"] = max(310, min(370, self.state["bat_volt"]))
        self.state["bat_curr"] = -120 + (self.state["vehicle_speed"] / 130) * -80
        self.state["bat_curr"] += random.uniform(-5, 5)

        # SOC 缓慢下降 (长途旅行) - 每 30s 减 1%
        if tick % 600 == 0:
            self.state["bat_soc"] = max(0, self.state["bat_soc"] - 1)

        # 能量模式：高速时 engine boost
        self.state["energy_mode"] = 2

        # 燃油消耗（持续 engine boost）
        self.state["fuel_level"] = max(0, self.state["fuel_level"] - 0.005)
        self.state["engine_rpm"] = int(self.state["vehicle_speed"] * 30)

        # 续航计算
        self.state["ev_range"] = max(0, int(self.state["bat_soc"] * 1.0))
        self.state["fuel_range"] = max(0, int(self.state["fuel_level"] * 8))

    def update_low_battery(self, tick: int):
        """低电量场景：SOC=6% 触发 bat_soc_low + soc_critical_low 报警
        模拟跛行模式 (limp home): 车速限制在 30km/h 以下
        """
        # 跛行模式：低 SOC 时车速限制
        # 用缓慢正弦模拟跛行中偶发小幅加速
        phase = (tick % 200) / 200.0
        if phase < 0.3:
            # 短暂尝试加速到 30km/h
            self.state["vehicle_speed"] = (phase / 0.3) * 30
        elif phase < 0.7:
            # 维持 30km/h
            self.state["vehicle_speed"] = 30
        else:
            # 减速到 0
            self.state["vehicle_speed"] = (1.0 - (phase - 0.7) / 0.3) * 30
        self.state["vehicle_speed"] = max(0, min(30, self.state["vehicle_speed"]))
        self.state["vehicle_speed"] += random.uniform(-0.5, 0.5)
        self.state["vehicle_speed"] = max(0, min(30, self.state["vehicle_speed"]))

        # 刹车 / 油门
        self.state["brake"] = 1 if self.state["vehicle_speed"] < 5 else 0

        # 电机低转速
        self.state["motor_rpm"] = int(self.state["vehicle_speed"] * 50)
        self.state["motor_temp"] = 40 + (self.state["vehicle_speed"] / 30) * 10
        self.state["motor_temp"] += random.uniform(-0.5, 0.5)

        # 电池低电压、小电流
        self.state["bat_volt"] = 320 + (self.state["bat_soc"] - 5) * 0.5
        self.state["bat_volt"] += random.uniform(-0.3, 0.3)
        self.state["bat_curr"] = -20 + (self.state["vehicle_speed"] / 30) * -10
        self.state["bat_curr"] += random.uniform(-2, 2)

        # SOC 维持在 6% 附近，偶尔抖动
        # 实际上 SOC 6% 已经很危险, 模拟不再继续充电
        if tick % 1000 == 0:
            self.state["bat_soc"] = max(0, self.state["bat_soc"] - 1)

        # 能量模式：低 SOC 时强制 EV
        self.state["energy_mode"] = 0

        # 续航计算
        self.state["ev_range"] = max(0, int(self.state["bat_soc"] * 1.0))
        self.state["fuel_range"] = int(self.state["fuel_level"] * 8)

    def update_state(self, tick: int):
        """根据 scenario 名字调用对应的 update 方法"""
        if self.scenario.name == "driving":
            self.update_driving(tick)
        elif self.scenario.name == "charging":
            self.update_charging(tick)
        elif self.scenario.name == "fault":
            self.update_fault(tick)
        elif self.scenario.name == "idle":
            self.update_idle(tick)
        elif self.scenario.name == "highway":
            self.update_highway(tick)
        elif self.scenario.name == "low_battery":
            self.update_low_battery(tick)
        else:
            self.update_driving(tick)  # default

    def run(self):
        sock = self.connect()
        self.running = True
        print(f"[CanSim] Connected to {self.socket_path} (scenario={self.scenario.name})")
        print(f"[CanSim] Initial state: bat_volt={self.state['bat_volt']:.1f}V "
              f"bat_soc={self.state['bat_soc']}% motor_temp={self.state['motor_temp']:.1f}°C")

        tick = 0
        while self.running:
            tick += 1
            self.update_state(tick)

            # ─── BMS 帧 0x186040F3 (100ms) ───
            if tick % 2 == 0:
                bat_volt_raw = int(self.state["bat_volt"] * 10)
                bat_curr_raw = int(self.state["bat_curr"] * 10 + 10000)
                bat_soc = max(0, min(100, int(self.state["bat_soc"])))
                data = struct.pack("<HHb", bat_volt_raw, bat_curr_raw, bat_soc)
                self.send_frame(sock, 0x186040F3, data)

            # ─── VCPU 帧 0x203 (50ms) ───
            if tick % 1 == 0:
                speed_raw = int(self.state["vehicle_speed"] * 10)
                data = struct.pack("<bHH", 0, self.state["brake"], speed_raw)
                self.send_frame(sock, 0x203, data)

            # ─── MCU 帧 0x101 (100ms) ───
            if tick % 2 == 1:
                motor_temp_raw = int(self.state["motor_temp"] + 40)
                data = struct.pack("<hhB", int(self.state["motor_rpm"]), 0, motor_temp_raw)
                self.send_frame(sock, 0x101, data)

            # ─── 发动机 0x305 (200ms) ───
            if tick % 4 == 0:
                engine_rpm_raw = int(self.state.get("engine_rpm", 0))
                data = struct.pack("<hB", engine_rpm_raw, int(self.state.get("engine_fault", 0)))
                self.send_frame(sock, 0x305, data)

            # ─── 充电 0x306/0x302 (200ms) ───
            if tick % 4 == 1:
                data = struct.pack("<BB",
                    int(self.state.get("charge_status", 0)),
                    int(self.state.get("charge_fault", 0)))
                self.send_frame(sock, 0x306, data)

            if tick % 4 == 2:
                power_raw = int(self.state.get("charge_power", 0) * 10)
                data = struct.pack("<h", power_raw)
                self.send_frame(sock, 0x302, data)

            # ─── 能量模式 0x300 (200ms) ───
            if tick % 4 == 3:
                data = struct.pack("<B", int(self.state.get("energy_mode", 0)))
                self.send_frame(sock, 0x300, data)

            # ─── 续航 + 燃油 (500ms) ───
            if tick % 10 == 0:
                ev_range = int(self.state.get("ev_range", 0))
                fuel_level = max(0, min(100, int(self.state.get("fuel_level", 0))))
                fuel_range = int(self.state.get("fuel_range", 0))
                data = struct.pack("<BHH", 0, fuel_range, ev_range)
                self.send_frame(sock, 0x307, data)
                data = struct.pack("<BBH", 0, fuel_level, fuel_range)
                self.send_frame(sock, 0x308, data)

            # ─── 档位 0x309 (500ms) ───
            if tick % 10 == 5:
                gear = int(self.state.get("gear_status", 0))
                data = struct.pack("<B", gear)
                self.send_frame(sock, 0x309, data)

            # ─── 胎压 0x3A0-0x3A3 (500ms) ───
            if tick % 10 == 7:
                fl = max(0, min(5, int(self.state["tire_pressure_fl"] * 100)))
                fr = max(0, min(5, int(self.state["tire_pressure_fr"] * 100)))
                rl = max(0, min(5, int(self.state["tire_pressure_rl"] * 100)))
                rr = max(0, min(5, int(self.state["tire_pressure_rr"] * 100)))
                data = struct.pack("<BB", fl, fr)
                self.send_frame(sock, 0x3A0, data)
                data = struct.pack("<B", fr)
                self.send_frame(sock, 0x3A1, data)
                data = struct.pack("<B", rl)
                self.send_frame(sock, 0x3A2, data)
                data = struct.pack("<B", rr)
                self.send_frame(sock, 0x3A3, data)

            # ─── 座椅占用帧 0x2F0/0x2F1 (500ms) ───
            if tick % 10 == 8:
                driver_occ = self.state["driver_occupied"] | (self.state["passenger_occupied"] << 1)
                self.send_frame(sock, 0x2F0, bytes([driver_occ]))
                self.send_frame(sock, 0x2F1, bytes([self.state["passenger_occupied"]]))

            # ─── 安全带帧 0x3B0/0x3B1/0x3B2 (200ms) ───
            if tick % 4 == 0:
                driver_buckle = self.state["driver_buckled"]
                passenger_buckle = self.state["passenger_buckled"] if self.state["passenger_occupied"] else 0
                self.send_frame(sock, 0x3B0, bytes([driver_buckle]))
                self.send_frame(sock, 0x3B1, bytes([passenger_buckle]))
                self.send_frame(sock, 0x3B2, bytes([self.state["rear_buckle"]]))

            time.sleep(0.05)  # 50ms 周期

        sock.close()
        print("[CanSim] Disconnected")


def main():
    parser = argparse.ArgumentParser(description="CAN 仿真引擎")
    parser.add_argument("--scenario", default="driving",
                        choices=["driving", "charging", "fault", "idle", "highway", "low_battery"],
                        help="仿真场景 (default: driving)")
    parser.add_argument("--duration", type=int, default=0,
                        help="运行时长（秒），0=无限 (default: 0)")
    args = parser.parse_args()

    sim = CanSimulator(scenario_name=args.scenario)
    reconnect_delay = 1.0
    max_delay = 5.0
    start_time = time.time()
    try:
        while True:
            if args.duration > 0 and (time.time() - start_time) > args.duration:
                print(f"[CanSim] Duration {args.duration}s reached, stopping.")
                break
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
