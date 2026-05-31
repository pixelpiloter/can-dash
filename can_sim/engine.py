#!/usr/bin/env python3
"""
CAN 仿真引擎
通过 Unix Socket 向 can-dash 发送模拟 CAN 帧

Usage:
    python can_sim/engine.py                    # 默认：普通驾驶场景
    python can_sim/engine.py --scenario normal  # 普通驾驶
    python can_sim/engine.py --scenario low_soc # 电量低（触发 bat_soc_low 报警）
    python can_sim/engine.py --scenario overvolt # 电池过压（触发 bat_overvolt 报警）
    python can_sim/engine.py --scenario undervolt # 电池欠压（触发 bat_undervolt 报警）
    python can_sim/engine.py --scenario overspeed # 超速（触发 overspeed 报警）
    python can_sim/engine.py --scenario motor_overtemp # 电机过热（触发 motor_overtemp 报警）
    python can_sim/engine.py --scenario timeout  # 信号中断（触发信号超时）
    python can_sim/engine.py --scenario reverse  # 倒车场景
    python can_sim/engine.py --scenario all      # 顺序运行所有场景
"""

import socket
import struct
import time
import random
import argparse
import sys
from dataclasses import dataclass, field
from typing import Dict, List, Callable, Optional

SOCKET_PATH = "/tmp/can_dash_socket"

# ─── 报警阈值（与 alarm_rules.yaml 保持一致）───
BAT_OVERVOLT_THRESHOLD = 420.0
BAT_UNDERVOLT_THRESHOLD = 280.0
BAT_SOC_LOW_THRESHOLD = 10.0
MOTOR_OVERTEMP_THRESHOLD = 120.0
OVERSPEED_THRESHOLD = 120.0


@dataclass
class Scenario:
    name: str
    description: str
    duration_sec: float = 30.0
    init_state: Dict = field(default_factory=dict)
    tick_callback: Optional[Callable] = None


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
            "driver_occupied": 1,
            "driver_buckled": 1,
            "passenger_occupied": 0,
            "passenger_buckled": 0,
            "rear_buckle": 0b00111,
        }

    def connect(self) -> socket.socket:
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.connect(self.socket_path)
        return sock

    def send_frame(self, sock: socket.socket, can_id: int, data: bytes):
        # Unix Socket 格式：[can_id(4bytes LE)][dlc(1byte)][data]
        msg = struct.pack("<IB", can_id, len(data)) + data
        sock.send(msg)

    def send_bms_frame(self, sock: socket.socket):
        """发送 BMS 帧 0x186040F3"""
        bat_volt_raw = int(self.state["bat_volt"] * 10)
        bat_curr_raw = int(self.state["bat_curr"] * 10 + 10000)
        data = struct.pack("<HHb", bat_volt_raw, bat_curr_raw, self.state["bat_soc"])
        self.send_frame(sock, 0x186040F3, data)

    def send_vcpu_frame(self, sock: socket.socket):
        """发送 VCPU 帧 0x203"""
        speed_raw = int(self.state["vehicle_speed"] * 10)
        data = struct.pack("<bHH", 0, self.state["brake"], speed_raw)
        self.send_frame(sock, 0x203, data)

    def send_mcu_frame(self, sock: socket.socket):
        """发送 MCU 帧 0x101"""
        motor_temp_raw = int(self.state["motor_temp"] + 40)
        data = struct.pack("<hhB", self.state["motor_rpm"], 0, motor_temp_raw)
        self.send_frame(sock, 0x101, data)

    def send_seat_frames(self, sock: socket.socket):
        """发送座椅占用帧 0x2F0/0x2F1"""
        driver_occ = self.state["driver_occupied"] | (self.state["passenger_occupied"] << 1)
        self.send_frame(sock, 0x2F0, bytes([driver_occ]))
        self.send_frame(sock, 0x2F1, bytes([self.state["passenger_occupied"]]))

    def send_seatbelt_frames(self, sock: socket.socket):
        """发送安全带帧 0x3B0/0x3B1/0x3B2"""
        driver_buckle = self.state["driver_buckled"]
        passenger_buckle = self.state["passenger_buckled"] if self.state["passenger_occupied"] else 0
        self.send_frame(sock, 0x3B0, bytes([driver_buckle]))
        self.send_frame(sock, 0x3B1, bytes([passenger_buckle]))
        self.send_frame(sock, 0x3B2, bytes([self.state["rear_buckle"]]))

    def log_status(self, tick: int, interval_ticks: int = 20):
        """每 N 个 tick 打印一次状态"""
        if tick % interval_ticks == 0:
            print(f"  [t={tick}] volt={self.state['bat_volt']:.1f}V "
                  f"curr={self.state['bat_curr']:.1f}A "
                  f"soc={self.state['bat_soc']}% "
                  f"spd={self.state['vehicle_speed']:.0f}km/h "
                  f"rpm={self.state['motor_rpm']} "
                  f"temp={self.state['motor_temp']:.0f}°C")

    # ─── 场景定义 ───────────────────────────────────────────────

    def scenario_normal(self, sock: socket.socket, tick: int, total_ticks: int):
        """普通驾驶：加速 → 巡航 → 减速 → 停止，循环"""
        cycle_pos = tick % 400  # 20秒一个完整周期

        if cycle_pos < 100:
            # 加速阶段 0-5s: 0 → 80 km/h
            t = cycle_pos / 100.0
            self.state["vehicle_speed"] = t * 80.0
            self.state["brake"] = 0
        elif cycle_pos < 300:
            # 巡航阶段 5-15s: 80 km/h，小幅波动
            self.state["vehicle_speed"] = 80.0 + random.uniform(-3, 5)
            self.state["brake"] = 0
        elif cycle_pos < 360:
            # 减速阶段 15-18s: 80 → 20 km/h
            t = (cycle_pos - 300) / 60.0
            self.state["vehicle_speed"] = 80.0 - t * 60.0
            self.state["brake"] = 30 if self.state["vehicle_speed"] > 40 else 0
        else:
            # 停止阶段 18-20s
            self.state["vehicle_speed"] = max(0, self.state["vehicle_speed"] - 3.0)
            self.state["brake"] = 50 if self.state["vehicle_speed"] > 5 else 0

        # 电压电流正常波动
        self.state["bat_volt"] += random.uniform(-0.3, 0.4)
        self.state["bat_volt"] = max(320, min(380, self.state["bat_volt"]))
        self.state["bat_curr"] += random.uniform(-3, 3)
        self.state["bat_curr"] = max(-200, min(100, self.state["bat_curr"]))

        # SOC 缓慢下降
        if tick % 200 == 0:
            self.state["bat_soc"] = max(0, self.state["bat_soc"] - 1)

        # 电机温度
        self.state["motor_temp"] += random.uniform(-0.3, 0.8)
        self.state["motor_temp"] = max(35, min(90, self.state["motor_temp"]))

        # 转速与车速成正比
        self.state["motor_rpm"] = int(self.state["vehicle_speed"] * 50)

    def scenario_low_soc(self, sock: socket.socket, tick: int, total_ticks: int):
        """电量低：SOC 从 15% 缓慢下降到 5%，触发 bat_soc_low 报警"""
        if tick == 0:
            self.state["bat_soc"] = 15
            self.state["bat_volt"] = 320.0
            print("  [场景] 电量低：SOC=15% → 5%，等待触发 bat_soc_low (value < 10)")

        # SOC 每秒下降 0.5%
        if tick % 20 == 0:
            self.state["bat_soc"] = max(0, self.state["bat_soc"] - 0.5)
            print(f"  [SOC] {self.state['bat_soc']:.1f}%")

        # 电压随 SOC 下降
        self.state["bat_volt"] = 300.0 + self.state["bat_soc"] * 1.5 + random.uniform(-1, 1)
        self.state["bat_curr"] = -30.0 + random.uniform(-5, 5)

        # 慢速行驶
        self.state["vehicle_speed"] = 30.0 + random.uniform(-5, 5)
        self.state["brake"] = 0
        self.state["motor_rpm"] = int(self.state["vehicle_speed"] * 50)
        self.state["motor_temp"] = 50.0 + random.uniform(-2, 3)

    def scenario_overvolt(self, sock: socket.socket, tick: int, total_ticks: int):
        """电池过压：电压从 380V 逐渐升至 430V，触发 bat_overvolt 报警"""
        if tick == 0:
            self.state["bat_volt"] = 380.0
            self.state["bat_soc"] = 85
            print("  [场景] 电池过压：380V → 430V，触发 bat_overvolt (value > 420)")

        # 每秒上升 5V
        if tick % 20 == 0:
            self.state["bat_volt"] += 5.0
            print(f"  [VOLT] {self.state['bat_volt']:.1f}V")

        if self.state["bat_volt"] > 430:
            self.state["bat_volt"] = 430.0  # 上限

        self.state["bat_curr"] = random.uniform(-20, 20)
        self.state["bat_soc"] = 80
        self.state["vehicle_speed"] = 60.0 + random.uniform(-10, 10)
        self.state["brake"] = 0
        self.state["motor_rpm"] = int(self.state["vehicle_speed"] * 50)
        self.state["motor_temp"] = 55.0 + random.uniform(-2, 3)

    def scenario_undervolt(self, sock: socket.socket, tick: int, total_ticks: int):
        """电池欠压：电压从 320V 逐渐降至 260V，触发 bat_undervolt 报警"""
        if tick == 0:
            self.state["bat_volt"] = 320.0
            self.state["bat_soc"] = 20
            print("  [场景] 电池欠压：320V → 260V，触发 bat_undervolt (value < 280)")

        # 每秒下降 6V
        if tick % 20 == 0:
            self.state["bat_volt"] -= 6.0
            print(f"  [VOLT] {self.state['bat_volt']:.1f}V")

        if self.state["bat_volt"] < 260:
            self.state["bat_volt"] = 260.0  # 下限

        self.state["bat_curr"] = -80.0 + random.uniform(-10, 10)
        self.state["bat_soc"] = max(0, self.state["bat_soc"] - 0.3)
        self.state["vehicle_speed"] = 40.0 + random.uniform(-10, 10)
        self.state["brake"] = 0
        self.state["motor_rpm"] = int(self.state["vehicle_speed"] * 50)
        self.state["motor_temp"] = 50.0 + random.uniform(-2, 3)

    def scenario_overspeed(self, sock: socket.socket, tick: int, total_ticks: int):
        """超速：车速从 80km/h 加速到 140km/h，触发 overspeed 报警"""
        if tick == 0:
            self.state["vehicle_speed"] = 80.0
            print("  [场景] 超速：80km/h → 140km/h，触发 overspeed (value > 120)")

        # 持续加速
        if tick % 20 == 0 and self.state["vehicle_speed"] < 140:
            self.state["vehicle_speed"] += 10.0
            print(f"  [SPD] {self.state['vehicle_speed']:.0f} km/h")

        self.state["vehicle_speed"] = min(140.0, self.state["vehicle_speed"] + 0.5)
        self.state["brake"] = 0
        self.state["motor_rpm"] = int(self.state["vehicle_speed"] * 50)

        # 电压电流正常
        self.state["bat_volt"] += random.uniform(-0.5, 0.5)
        self.state["bat_volt"] = max(340, min(380, self.state["bat_volt"]))
        self.state["bat_curr"] = random.uniform(-100, 50)
        self.state["bat_soc"] = 70

        # 电机温度随负载上升
        self.state["motor_temp"] += 0.1
        self.state["motor_temp"] = min(130, self.state["motor_temp"])

    def scenario_motor_overtemp(self, sock: socket.socket, tick: int, total_ticks: int):
        """电机过热：温度从 80°C 升至 130°C，触发 motor_overtemp 报警"""
        if tick == 0:
            self.state["motor_temp"] = 80.0
            self.state["vehicle_speed"] = 100.0
            print("  [场景] 电机过热：80°C → 130°C，触发 motor_overtemp (value > 120)")

        # 温度持续上升
        if tick % 20 == 0:
            self.state["motor_temp"] += 5.0
            print(f"  [TEMP] {self.state['motor_temp']:.0f}°C")

        self.state["motor_temp"] = min(130.0, self.state["motor_temp"] + 0.2)
        self.state["motor_rpm"] = int(self.state["vehicle_speed"] * 50)
        self.state["bat_volt"] = 360.0 + random.uniform(-5, 5)
        self.state["bat_curr"] = random.uniform(-150, 50)
        self.state["bat_soc"] = 65

        # 车速维持高位，加剧过热
        self.state["vehicle_speed"] = 100.0 + random.uniform(-10, 15)
        self.state["brake"] = 0

    def scenario_timeout(self, sock: socket.socket, tick: int, total_ticks: int):
        """信号中断：5秒后停止发送所有帧，模拟 CAN 信号丢失"""
        if tick == 0:
            print("  [场景] 信号中断：前5秒正常，之后停止发送所有帧")

        # 前 5 秒正常发送（tick 0-99，100 ticks = 5秒 @ 50ms）
        if tick >= 100:
            # 信号中断：不发送任何帧，但保持 socket 连接
            if tick == 100:
                print("  [TIMEOUT] 信号中断开始，停止发送 CAN 帧...")
            # 空循环，等待超时被触发
            time.sleep(0.05)
            return

        # 前 5 秒：正常驾驶状态
        self.state["vehicle_speed"] = 60.0 + random.uniform(-5, 5)
        self.state["brake"] = 0
        self.state["motor_rpm"] = int(self.state["vehicle_speed"] * 50)
        self.state["motor_temp"] = 55.0 + random.uniform(-2, 3)
        self.state["bat_volt"] = 360.0 + random.uniform(-3, 3)
        self.state["bat_curr"] = random.uniform(-50, 20)
        self.state["bat_soc"] = 70

    def scenario_reverse(self, sock: socket.socket, tick: int, total_ticks: int):
        """倒车场景：车速为负或极低，模拟倒车状态"""
        if tick == 0:
            print("  [场景] 倒车：低速倒车，安全带未系测试")

        # 低速倒车
        self.state["vehicle_speed"] = -5.0 + random.uniform(-2, 1)  # 约 -5 km/h
        self.state["brake"] = 30
        self.state["motor_rpm"] = int(abs(self.state["vehicle_speed"]) * 50)
        self.state["motor_temp"] = 40.0 + random.uniform(-1, 2)

        self.state["bat_volt"] = 350.0 + random.uniform(-3, 3)
        self.state["bat_curr"] = -30.0 + random.uniform(-10, 10)
        self.state["bat_soc"] = 60

        # 安全带未系（测试报警）
        if tick % 100 == 0:
            self.state["driver_buckled"] = 0
            print(f"  [SEATBELT] driver_buckled={self.state['driver_buckled']}")

    def run_scenario(self, scenario_name: str, duration_sec: float = 30.0):
        """运行单个场景"""
        sock = self.connect()
        self.running = True

        # 重置状态到初始值
        self.state = {
            "bat_volt": 350.0, "bat_curr": -50.0, "bat_soc": 75,
            "vehicle_speed": 0.0, "brake": 0, "motor_rpm": 0, "motor_temp": 45,
            "driver_occupied": 1, "driver_buckled": 1,
            "passenger_occupied": 0, "passenger_buckled": 0,
            "rear_buckle": 0b00111,
        }

        print(f"\n{'='*50}")
        print(f"[CanSim] 场景: {scenario_name}")
        print(f"[CanSim] 连接 {self.socket_path}")
        print(f"{'='*50}")

        tick = 0
        end_tick = int(duration_sec / 0.05)  # 50ms per tick

        scenario_method = getattr(self, f"scenario_{scenario_name}", None)
        if scenario_method is None:
            print(f"[CanSim] 未知场景: {scenario_name}")
            sock.close()
            return

        while self.running and tick < end_tick:
            tick += 1

            # 调用场景逻辑
            scenario_method(sock, tick, end_tick)

            # 只有 timeout 场景会在 tick>=100 后停止发送
            skip_frames = (scenario_name == "timeout" and tick >= 100)

            if not skip_frames:
                # 发送所有 CAN 帧
                # BMS (100ms = tick % 10)
                if tick % 10 == 0:
                    self.send_bms_frame(sock)
                # VCPU (50ms = tick % 5)
                if tick % 5 == 0:
                    self.send_vcpu_frame(sock)
                # MCU (100ms = tick % 10, offset by 5)
                if tick % 10 == 5:
                    self.send_mcu_frame(sock)
                # 座椅 (500ms = tick % 50)
                if tick % 50 == 0:
                    self.send_seat_frames(sock)
                # 安全带 (200ms = tick % 20)
                if tick % 20 == 0:
                    self.send_seatbelt_frames(sock)

                self.log_status(tick)

            time.sleep(0.05)

        sock.close()
        print(f"[CanSim] 场景结束: {scenario_name}")
        print(f"  最终状态: volt={self.state['bat_volt']:.1f}V "
              f"soc={self.state['bat_soc']}% "
              f"spd={self.state['vehicle_speed']:.0f}km/h "
              f"temp={self.state['motor_temp']:.0f}°C")


def main():
    parser = argparse.ArgumentParser(
        description="CAN 仿真引擎 - 支持多场景测试",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
场景说明:
  normal       普通驾驶循环（加速/巡航/减速/停止）
  low_soc      电量低报警（SOC < 10%%）
  overvolt     电池过压报警（bat_volt > 420V）
  undervolt    电池欠压报警（bat_volt < 280V）
  overspeed    超速报警（speed > 120km/h）
  motor_overtemp  电机过热报警（motor_temp > 120°C）
  timeout      信号中断（5秒后停止发帧，测试超时）
  reverse      倒车场景（低速倒车，安全带未系）
  all          依次运行所有场景

示例:
  python can_sim/engine.py                    # 默认普通驾驶
  python can_sim/engine.py --scenario overspeed  # 超速场景
  python can_sim/engine.py --scenario all     # 运行所有场景
  python can_sim/engine.py --duration 60 --scenario low_soc  # 60秒电量低场景
"""
    )
    parser.add_argument(
        "--scenario", "-s",
        default="normal",
        choices=["normal", "low_soc", "overvolt", "undervolt",
                 "overspeed", "motor_overtemp", "timeout", "reverse", "all"],
        help="仿真场景 (default: normal)"
    )
    parser.add_argument(
        "--duration", "-d",
        type=float, default=30.0,
        help="单场景持续秒数 (default: 30)"
    )
    args = parser.parse_args()

    if args.scenario == "all":
        scenarios = ["normal", "low_soc", "overvolt", "undervolt",
                     "overspeed", "motor_overtemp", "timeout", "reverse"]
        print(f"[CanSim] 将依次运行 {len(scenarios)} 个场景...")
        for scen in scenarios:
            sim = CanSimulator()
            reconnect_delay = 1.0
            success = False
            while not success:
                try:
                    sim.run_scenario(scen, duration_sec=args.duration)
                    success = True
                except (BrokenPipeError, ConnectionRefusedError, OSError) as e:
                    print(f"[CanSim] 连接失败: {e}，{reconnect_delay:.1f}s 后重试...")
                    time.sleep(reconnect_delay)
                    reconnect_delay = min(5.0, reconnect_delay * 1.5)
                    sim = CanSimulator()
                except KeyboardInterrupt:
                    print("\n[CanSim] 被用户中断")
                    return
    else:
        sim = CanSimulator()
        reconnect_delay = 1.0
        while True:
            try:
                sim.run_scenario(args.scenario, duration_sec=args.duration)
                break
            except (BrokenPipeError, ConnectionRefusedError, OSError) as e:
                print(f"[CanSim] 连接失败: {e}，{reconnect_delay:.1f}s 后重试...")
                time.sleep(reconnect_delay)
                reconnect_delay = min(5.0, reconnect_delay * 1.5)
                sim = CanSimulator()
            except KeyboardInterrupt:
                print("\n[CanSim] 停止")
                break


if __name__ == "__main__":
    main()
