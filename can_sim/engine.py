#!/usr/bin/env python3
"""
CAN 仿真引擎
通过 Unix Socket 向 can-dash 发送模拟 CAN 帧

Usage:
    python can_sim/engine.py [--scenario driving|charging|fault|idle|highway|low_battery|hybrid|regen|thermal] [--duration 30]

Scenarios:
    driving      - 正常驾驶循环（默认）：加速 → 巡航 → 减速
    charging     - 充电场景：bat_curr 负、charge_status=1、vehicle_speed=0
    fault        - 故障注入：触发电机超温 + 发动机故障 + 低胎压
    idle         - 静止：所有信号保持初始值
    highway      - 高速巡航：持续 120-130km/h, energy_mode=2 (engine boost), 高功耗
    low_battery  - 低电量：SOC<10% 触发 bat_soc_low + soc_critical_low 报警, 跛行降速
    hybrid       - 混合驱动：能量模式 HYBRID (energy_mode=1), 电机+发动机同时工作
    regen        - 能量回收：高速→刹车减速, bat_curr 变正 (充电), charge_power > 0, SOC 微涨
    limp_home    - 跛行模式：engine_fault=1, 限速 50km/h, charge_fault=0, 验证 LimpHomePanel
    thermal      - 电池热失控：重载低速→battery_temp 升至 65/75°C, 触发 bat_thermal warning/critical
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
    elif name == "hybrid":
        # 混合驱动场景：能量模式 HYBRID (1)
        # 电机 + 发动机 同时工作, 中等车速, SOC 适中, 燃油消耗与电量消耗同时存在
        # 用途: 验证 energy_mode=1 时的 EnergyFlowDiagram 渲染
        s.initial.update({
            "vehicle_speed": 60,      # 中速巡航
            "bat_soc": 55,            # 中等 SOC
            "bat_volt": 355,          # 正常电压
            "bat_curr": -30,          # 小幅放电 (电机驱动)
            "energy_mode": 1,         # HYBRID
            "engine_rpm": 1800,       # 发动机运行中
            "motor_rpm": 3000,        # 电机运行
            "motor_temp": 65,         # 中等温度
            "brake": 0,               # 不踩刹车
            "fuel_level": 55,         # 燃油中等
            "charge_status": 0,
            "charge_power": 0,
        })
    elif name == "regen":
        # 能量回收场景：高速→急减速, 电机反转作发电机
        # bat_curr 变正 (充电), charge_power > 0, SOC 微涨
        # 用途: 验证 charge_power>0 + bat_curr>0 + EnergyFlowDiagram 充电动画
        s.initial.update({
            "vehicle_speed": 80,      # 起始高速 (后续 update_regen 减速)
            "bat_soc": 60,            # 中等 SOC (回收后微涨)
            "bat_volt": 355,
            "bat_curr": -50,          # 起始为放电, update_regen 会变正
            "energy_mode": 0,         # EV (无发动机)
            "engine_rpm": 0,          # 纯电回收
            "engine_fault": 0,
            "motor_rpm": 4000,        # 高速对应高 RPM
            "motor_temp": 55,
            "brake": 0,               # 起始未踩, 减速时切 1
            "fuel_level": 60,
            "charge_status": 0,       # 0 = 未插枪 (regen ≠ 充电桩充电)
            "charge_power": 0,        # update_regen 会让它 > 0
            "charge_fault": 0,
        })
    elif name == "limp_home":
        # 跛行模式场景：发动机故障 → 限速 50km/h 保护
        # engine_fault=1 (触发 LimpHomeRuntime 进入 ACTIVE 级别)
        # 车速上限 50km/h, 验证 LimpHomePanel 显示
        # 周期 240 ticks (12s @ 50ms): 0~80 加速, 80~160 巡航 50, 160~240 减速
        s.initial.update({
            "vehicle_speed": 0,        # 起始 0
            "bat_soc": 35,             # 中等电量
            "bat_volt": 350,
            "bat_curr": -30,           # 适度放电
            "energy_mode": 0,          # EV 模式 (跛行时纯电驱动)
            "engine_rpm": 0,           # 跛行时发动机停机
            "engine_fault": 1,         # 关键: 发动机故障 → 跛行
            "motor_rpm": 0,
            "motor_temp": 65,          # 电机温度正常
            "brake": 0,
            "fuel_level": 50,
            "charge_status": 0,
            "charge_power": 0,
            "charge_fault": 0,
        })
    elif name == "thermal":
        # 电池热失控场景: 重载低速 → 电池持续大电流放电 → battery_temp 升至危险区
        # 用途: 验证 alarm_rules.yaml 的 bat_thermal (battery_temp>65) 和
        #       bat_thermal_critical (battery_temp>75) 报警触发/恢复
        # 周期 500 ticks (25s @ 50ms): 5 段每段 100 ticks = 5s
        #   0~100  (5s):  急加速 + 大电流放电,  temp 30→55°C
        #   100~200(5s):  持续低速重载,         temp 55→68°C  (触发 bat_thermal)
        #   200~300(5s):  极限放电,            temp 68→80°C  (触发 bat_thermal_critical)
        #   300~400(5s):  减速停驶,            temp 80→70°C  (critical 解除, warning 仍触发)
        #   400~500(5s):  静止冷却,            temp 70→58°C  (warning 解除, 验证恢复)
        s.initial.update({
            "vehicle_speed": 0,        # 起始 0
            "bat_soc": 60,             # 中等电量 (重载放电用)
            "bat_volt": 360,           # 正常电压
            "bat_curr": -100,          # 重载放电 (update_thermal 会进一步拉大)
            "battery_temp": 30,        # 起始常温
            "energy_mode": 0,          # EV 模式 (纯电, 电池单独工作, 升温快)
            "engine_rpm": 0,
            "engine_fault": 0,
            "motor_rpm": 0,
            "motor_temp": 40,          # 电机温度低 (起始)
            "brake": 0,
            "fuel_level": 50,
            "charge_status": 0,
            "charge_power": 0,
            "charge_fault": 0,
            # 胎压正常 (避免无关报警)
            "tire_pressure_fl": 2.3, "tire_pressure_fr": 2.3,
            "tire_pressure_rl": 2.3, "tire_pressure_rr": 2.3,
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
        elif self.scenario.name == "hybrid":
            self.update_hybrid(tick)
        elif self.scenario.name == "regen":
            self.update_regen(tick)
        elif self.scenario.name == "limp_home":
            self.update_limp_home(tick)
        elif self.scenario.name == "thermal":
            self.update_thermal(tick)
        else:
            self.update_driving(tick)  # default

    def update_hybrid(self, tick: int):
        """混合驱动场景：能量模式 HYBRID (energy_mode=1)
        电机 + 发动机 同时工作, 中等车速 50-70 km/h, SOC 适中
        周期 300 ticks (15s @ 50ms), 让 sparkline 看到完整波动
        """
        phase = (tick % 300) / 300.0
        # 速度在 50-70 之间慢起伏
        if phase < 0.5:
            speed = 50 + (phase / 0.5) * 20
        else:
            speed = 70 - ((phase - 0.5) / 0.5) * 20
        speed += random.uniform(-2, 2)
        self.state["vehicle_speed"] = max(0, min(260, speed))
        self.state["brake"] = 0  # 巡航, 不踩刹车

        # 电机：转速 = speed * 50, 温度中等
        self.state["motor_rpm"] = int(self.state["vehicle_speed"] * 50)
        self.state["motor_temp"] = 60 + (self.state["vehicle_speed"] / 70) * 15
        self.state["motor_temp"] += random.uniform(-0.5, 0.5)
        self.state["motor_temp"] = max(30, min(150, self.state["motor_temp"]))

        # 电池：稳定电压, 小幅放电 (电机驱动)
        self.state["bat_volt"] += random.uniform(-0.2, 0.2)
        self.state["bat_volt"] = max(340, min(370, self.state["bat_volt"]))
        self.state["bat_curr"] = -25 + (self.state["vehicle_speed"] / 70) * -15
        self.state["bat_curr"] += random.uniform(-3, 3)

        # SOC 缓慢下降 (电机 + 发动机混合消耗)
        if tick % 600 == 0:
            self.state["bat_soc"] = max(0, self.state["bat_soc"] - 1)

        # 发动机: 中等转速, 持续运行
        self.state["engine_rpm"] = 1500 + int(self.state["vehicle_speed"] * 5)
        self.state["engine_rpm"] += random.randint(-50, 50)
        self.state["engine_fault"] = 0

        # 能量模式：固定 HYBRID (1)
        self.state["energy_mode"] = 1

        # 燃油消耗 (发动机持续运行)
        self.state["fuel_level"] = max(0, self.state["fuel_level"] - 0.002)

        # 续航计算
        self.state["ev_range"] = max(0, int(self.state["bat_soc"] * 1.0))
        self.state["fuel_range"] = max(0, int(self.state["fuel_level"] * 8))

        # 充电状态
        self.state["charge_status"] = 0
        self.state["charge_fault"] = 0
        self.state["charge_power"] = 0

    def update_regen(self, tick: int):
        """能量回收场景：高速→急减速, 电机反转作发电机
        周期 240 ticks (12s @ 50ms):
          0~80   (4s): 维持高速 80km/h, 正常放电
          80~160 (4s): 急减速 80→0, 触发 regen (bat_curr 翻正, charge_power > 0)
          160~240(4s): 静止 (speed=0), 保持少量 regen 充能
        """
        phase = (tick % 240) / 240.0
        if phase < 0.333:
            # ── 阶段 1: 维持高速, 正常放电 ──
            speed = 80
            self.state["brake"] = 0
            # bat_curr 负 (放电), charge_power=0
            self.state["bat_curr"] = -50 + random.uniform(-5, 5)
            self.state["charge_power"] = 0
        elif phase < 0.667:
            # ── 阶段 2: 急减速 80→0, 触发 regen ──
            # 局部 phase (0..1) 表示 80→0 的进度
            local = (phase - 0.333) / 0.334
            speed = 80 * (1.0 - local)  # 80 → 0 线性
            speed = max(0, speed)
            self.state["brake"] = 1 if speed > 5 else 0  # 减速中踩刹车
            # regen 强度: 与瞬时减速度成正比 (这里简化为速度本身)
            regen_amp = speed * 1.5  # km/h → 充电电流
            self.state["bat_curr"] = regen_amp + random.uniform(-3, 3)
            self.state["charge_power"] = speed * 0.6  # kW
        else:
            # ── 阶段 3: 静止, 微充 (电机 freewheel) ──
            speed = 0
            self.state["brake"] = 0
            self.state["bat_curr"] = 5 + random.uniform(-1, 1)
            self.state["charge_power"] = 0.5 + random.uniform(-0.1, 0.1)
        # 小幅抖动
        speed += random.uniform(-0.5, 0.5)
        self.state["vehicle_speed"] = max(0, min(260, speed))

        # 电机: 高速时高 RPM, 减速时下降
        # regen 时电机 RPM 仍随轮速
        self.state["motor_rpm"] = int(self.state["vehicle_speed"] * 50)
        # 温度: regen 时电机也发热, 但比驱动时低
        if phase < 0.333:
            self.state["motor_temp"] = 55 + random.uniform(-0.5, 0.5)
        elif phase < 0.667:
            # 急减速时温度微涨 (regen 也有损耗)
            self.state["motor_temp"] = 58 + random.uniform(-0.5, 0.5)
        else:
            self.state["motor_temp"] = 56 + random.uniform(-0.3, 0.3)
        self.state["motor_temp"] = max(30, min(150, self.state["motor_temp"]))

        # 电池: 充电时电压微涨
        if self.state["bat_curr"] > 0:
            self.state["bat_volt"] = 355 + min(10, self.state["bat_curr"] * 0.05)
        else:
            self.state["bat_volt"] = 350 + random.uniform(-0.3, 0.3)
        self.state["bat_volt"] = max(310, min(370, self.state["bat_volt"]))

        # SOC: regen 时微涨 (每 5s +0.1, 非常慢)
        if self.state["bat_curr"] > 10 and tick % 100 == 0:
            self.state["bat_soc"] = min(100, self.state["bat_soc"] + 0.1)

        # 能量模式: 整个过程都是 EV
        self.state["energy_mode"] = 0
        self.state["engine_rpm"] = 0
        self.state["engine_fault"] = 0
        self.state["charge_status"] = 0   # 0 = regen (非充电桩)
        self.state["charge_fault"] = 0

        # 续航
        self.state["ev_range"] = max(0, int(self.state["bat_soc"] * 1.0))
        self.state["fuel_range"] = int(self.state["fuel_level"] * 8)

    def update_limp_home(self, tick: int):
        """跛行模式场景：engine_fault=1, 限速 50km/h
        周期 240 ticks (12s @ 50ms): 0~80 加速 0→50, 80~160 巡航 50, 160~240 减速 50→0
        用途: 验证 LimpHomePanel 显示和 LimpHomeRuntime state machine
        """
        phase = (tick % 240) / 240.0
        if phase < 0.333:
            # 加速 0 → 50
            speed = (phase / 0.333) * 50
        elif phase < 0.667:
            # 巡航 50 (限速)
            speed = 50
        else:
            # 减速 50 → 0
            speed = (1.0 - (phase - 0.667) / 0.333) * 50
        speed += random.uniform(-1, 1)  # 小幅抖动
        self.state["vehicle_speed"] = max(0, min(50, speed))  # 硬限速 50

        # 刹车: 减速时踩, 加速/巡航时不踩
        if phase >= 0.667:
            self.state["brake"] = 1 if self.state["vehicle_speed"] > 5 else 0
        else:
            self.state["brake"] = 0

        # 电机: 转速 = speed * 50 (跛行时低转速)
        self.state["motor_rpm"] = int(self.state["vehicle_speed"] * 50)
        # 温度: 跛行时电机不过载, 温度正常
        self.state["motor_temp"] = 55 + (self.state["vehicle_speed"] / 50) * 15
        self.state["motor_temp"] += random.uniform(-0.5, 0.5)
        self.state["motor_temp"] = max(30, min(150, self.state["motor_temp"]))

        # 电池: 适度放电
        self.state["bat_volt"] += random.uniform(-0.3, 0.3)
        self.state["bat_volt"] = max(330, min(370, self.state["bat_volt"]))
        self.state["bat_curr"] = -20 + (self.state["vehicle_speed"] / 50) * -30
        self.state["bat_curr"] += random.uniform(-3, 3)

        # SOC: 跛行时缓慢下降
        if tick % 800 == 0:
            self.state["bat_soc"] = max(0, self.state["bat_soc"] - 1)

        # 关键: 发动机故障, 发动机停机
        self.state["engine_fault"] = 1
        self.state["engine_rpm"] = 0
        self.state["fuel_level"] = max(0, self.state["fuel_level"] - 0.001)  # 仍消耗油 (系统待机)
        # 实际跛行时 engine 不工作, fuel_level 维持

        # 能量模式: 跛行时强制 EV (纯电)
        self.state["energy_mode"] = 0

        # 充电: 跛行时不允许充电
        self.state["charge_status"] = 0
        self.state["charge_fault"] = 0
        self.state["charge_power"] = 0

        # 续航
        self.state["ev_range"] = max(0, int(self.state["bat_soc"] * 1.0))
        self.state["fuel_range"] = int(self.state["fuel_level"] * 8)

    def update_thermal(self, tick: int):
        """电池热失控场景: 重载低速 → 电池持续大电流放电 → battery_temp 升至危险区
        周期 500 ticks (25s @ 50ms), 5 段每段 100 ticks = 5s:
          0~100  (5s):  急加速 + 大电流放电,  temp 30→55°C
          100~200(5s):  持续低速重载,         temp 55→68°C  (触发 bat_thermal warning)
          200~300(5s):  极限放电,            temp 68→80°C  (触发 bat_thermal critical)
          300~400(5s):  减速停驶,            temp 80→70°C  (critical 解除, warning 仍触发)
          400~500(5s):  静止冷却,            temp 70→58°C  (warning 解除, 验证恢复)
        用途: 验证 alarm_rules.yaml 的 bat_thermal (battery_temp>65) 和
              bat_thermal_critical (battery_temp>75) 报警触发/恢复
        """
        phase = (tick % 500) / 500.0

        # ── 车速: 加速→巡航低速→减速→静止 ──
        if phase < 0.2:
            # 阶段 1: 急加速 0→40 km/h
            speed = (phase / 0.2) * 40
        elif phase < 0.6:
            # 阶段 2~3: 持续低速重载 40 km/h
            speed = 40
        elif phase < 0.8:
            # 阶段 4: 减速 40→0
            local = (phase - 0.6) / 0.2
            speed = 40 * (1.0 - local)
        else:
            # 阶段 5: 静止冷却
            speed = 0
        speed += random.uniform(-0.5, 0.5)
        self.state["vehicle_speed"] = max(0, min(260, speed))

        # 刹车: 减速段 + 静止段 (这里为了简化, 静止段也算 brake=0)
        if 0.6 <= phase < 0.8 and self.state["vehicle_speed"] > 5:
            self.state["brake"] = 1
        else:
            self.state["brake"] = 0

        # ── battery_temp 演化: 5 段递进, 含充放热 ──
        # 阶段 1: 30→55 (升温 +0.5°C/tick = +10°C/s)
        # 阶段 2: 55→68 (升温 +0.26°C/tick = +5.2°C/s)
        # 阶段 3: 68→80 (升温 +0.24°C/tick = +4.8°C/s, 触顶)
        # 阶段 4: 80→70 (自然冷却 -0.2°C/tick = -4°C/s)
        # 阶段 5: 70→58 (静止冷却 -0.24°C/tick = -4.8°C/s, 仍 trigger warning)
        if phase < 0.2:
            # 阶段 1: 急加速
            self.state["battery_temp"] = 30 + (phase / 0.2) * 25  # 30→55
            self.state["bat_curr"] = -150 + (phase / 0.2) * -30  # 150A→180A 持续重载
        elif phase < 0.4:
            # 阶段 2: 持续重载 (warning 触发)
            local = (phase - 0.2) / 0.2
            self.state["battery_temp"] = 55 + local * 13  # 55→68
            self.state["bat_curr"] = -200  # 极限放电
        elif phase < 0.6:
            # 阶段 3: 持续极限 (critical 触发)
            local = (phase - 0.4) / 0.2
            self.state["battery_temp"] = 68 + local * 12  # 68→80
            self.state["bat_curr"] = -220  # 持续极限
        elif phase < 0.8:
            # 阶段 4: 减速, 电池开始自然冷却
            local = (phase - 0.6) / 0.2
            self.state["battery_temp"] = 80 - local * 10  # 80→70
            self.state["bat_curr"] = -50 + local * 30  # 50A→20A 减载
        else:
            # 阶段 5: 静止, 持续冷却, 验证 warning 解除
            local = (phase - 0.8) / 0.2
            self.state["battery_temp"] = 70 - local * 12  # 70→58
            self.state["bat_curr"] = -20  # 系统待机微耗电
        # 抖动
        self.state["battery_temp"] += random.uniform(-0.3, 0.3)
        self.state["battery_temp"] = max(20, min(90, self.state["battery_temp"]))

        # ── 电池: 电压随温度/SOC 变化 ──
        # 温度升高时电压略升 (内阻减小), 但 SOC 下降会拖低
        self.state["bat_volt"] = 355 + (self.state["battery_temp"] - 30) * 0.2
        self.state["bat_volt"] = max(330, min(370, self.state["bat_volt"]))
        self.state["bat_curr"] += random.uniform(-3, 3)
        # 大量放电时 SOC 下降
        if tick % 200 == 0 and self.state["bat_curr"] < -100:
            self.state["bat_soc"] = max(20, self.state["bat_soc"] - 1)

        # ── 电机: RPM = speed*50, 温度随负载上升 ──
        self.state["motor_rpm"] = int(self.state["vehicle_speed"] * 50)
        # 电机温度: 重载时上升, 静止时下降
        if phase < 0.6:
            self.state["motor_temp"] = 40 + (phase / 0.6) * 50  # 40→90
        elif phase < 0.8:
            self.state["motor_temp"] = 90 - ((phase - 0.6) / 0.2) * 20  # 90→70
        else:
            self.state["motor_temp"] = 70 - ((phase - 0.8) / 0.2) * 15  # 70→55
        self.state["motor_temp"] += random.uniform(-0.5, 0.5)
        self.state["motor_temp"] = max(30, min(150, self.state["motor_temp"]))

        # ── 发动机 / 充电: 全部正常 (热失控仅电池 + 电机) ──
        self.state["engine_rpm"] = 0
        self.state["engine_fault"] = 0
        self.state["energy_mode"] = 0  # EV
        self.state["charge_status"] = 0
        self.state["charge_fault"] = 0
        self.state["charge_power"] = 0

        # 续航
        self.state["ev_range"] = max(0, int(self.state["bat_soc"] * 1.0))
        self.state["fuel_range"] = int(self.state["fuel_level"] * 8)

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
                        choices=["driving", "charging", "fault", "idle",
                                 "highway", "low_battery", "hybrid", "regen",
                                 "limp_home", "thermal"],
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
