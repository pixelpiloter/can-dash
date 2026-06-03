// view_manager.h
// Layer 2: 视图模式状态机 (驾驶 / 充电 / 驻车设置)
// 纯 C++，无 Qt，无 YAML 运行时
//
// 职责:
//   - 根据 CAN 信号 (gear / charge_status) 决定当前应显示的视图模式
//   - 状态机: DRIVE / CHARGE / SETUP 三态 + hysteresis 防抖
//   - 暴露 getter 给 Layer 3 binder 读取, 供 QML 切视图
//
// 设计要点 (PR 11: L2 + test, 不接数据流):
//   - 触发源只有 2 个: gear_status (0=P, 1=R, 2=N, 3=D, 4=S)
//                      + charge_status (0=idle, 1+=charging)
//   - 优先级: CHARGE > DRIVE > SETUP (充电覆盖一切, 行驶次之, 驻车为默认)
//   - alarm_active 不参与 view 切换 — 报警在独立 QML 面板叠加 (避免告警把用户从驾驶视图弹走)
//   - hysteresis: 状态切换至少间隔 kDefaultHysteresisMs (1s), 避免信号抖动引发视图闪烁
//   - tick() 是推进时间 + 应用 pending transition 的入口
//   - setViewForTest() 绕过 hysteresis, 供测试 + 调试快速验证
//   - 默认 = DRIVE (项目以驾驶视图为主, 启动时 P 档不立即跳 SETUP)
//
// 复用现有模式 (参照 ThemeManager / TripComputer / WarningManager / SettingsManager):
//   - 纯 C++ 类, 状态自包含, 无 Qt
//   - tick() 由上游按需调用
//   - reset() 回到默认状态
//   - kDefault* 静态常量供测试和外部读取
//   - ViewSnapshot 一次返回全部字段, 跟其他 L2 业务组件对齐
//
// 不在本 PR 范围 (后续 PR 接入数据流 + QML 视图切换):
//   - 不修改 DisplaySnapshot / QtDataBinder / DashboardBackend
//   - 不加 QML 视图切换逻辑 (后续 PR 改 DashboardMain.qml + 加 ViewSwitcher.qml)
//   - 不持久化最后视图 (用户重启后回到默认 DRIVE, 简化首版)

#pragma once

#include <cstdint>
#include <climits>

namespace candash {

// 视图模式
enum class ViewMode : uint8_t {
    DRIVE  = 0,  // 驾驶视图 (默认)
    CHARGE = 1,  // 充电视图 (高优先级, 覆盖一切)
    SETUP  = 2,  // 驻车设置视图 (P 档且未充电)
};

// 当前 L2 支持的视图快照 (供 Layer 3 binder 一次性读取)
struct ViewSnapshot {
    uint8_t  current;     // ViewMode 枚举
    uint8_t  gear;        // 当前 gear_status (0=P, 1=R, 2=N, 3=D, 4=S)
    uint8_t  charge;      // 当前 charge_status
    uint8_t  _pad;        // 对齐到 4 字节
};

class ViewManager {
public:
    ViewManager();

    // ─── 输入 (由 ShmDataSource 桥接 CAN 信号, 后续 PR 接入) ───
    // gear: 0=P, 1=R, 2=N, 3=D, 4=S
    void setGearStatus(uint8_t gear);
    // charge: 0=idle, 1+=charging
    void setChargeStatus(uint8_t charge);
    // 显式 alarm 不影响 view (保留接口占位 + 测试 symmetry)
    void setAlarmActive(uint8_t active) { (void)active; }

    // ─── tick ───
    // 推进时间, 应用 pending transition (hysteresis 满了才真正切)
    void tick(uint64_t now_ms);

    // ─── 输出 ───
    ViewMode currentView() const { return m_current; }
    bool isDrive()  const { return m_current == ViewMode::DRIVE; }
    bool isCharge() const { return m_current == ViewMode::CHARGE; }
    bool isSetup()  const { return m_current == ViewMode::SETUP; }

    // 一次性导出 (供 Layer 3 binder 一次性读取, 避免多次 getter 调用)
    ViewSnapshot snapshot() const;

    // ─── 配置 ───
    void setHysteresisMs(uint64_t ms) { m_hysteresisMs = ms; }
    uint64_t hysteresisMs() const { return m_hysteresisMs; }

    // ─── 重置 ───
    void reset();

    // ─── 测试钩子 ───
    // 立即切到指定 view, 绕过 hysteresis (供 L2 单元测试 + Layer 3 调试)
    void setViewForTest(ViewMode v);

    // 默认值 (外部可读, 跨进程配置回退)
    static const ViewMode kDefaultView         = ViewMode::DRIVE;
    static const uint64_t kDefaultHysteresisMs  = 1000;  // 1s
    // gear 编码常量 (供 L3 / 测试知道语义)
    static const uint8_t  kGearPark    = 0;
    static const uint8_t  kGearReverse = 1;
    static const uint8_t  kGearNeutral = 2;
    static const uint8_t  kGearDrive   = 3;
    static const uint8_t  kGearSport   = 4;
    // charge 编码常量
    static const uint8_t  kChargeIdle      = 0;
    static const uint8_t  kChargeActive    = 1;  // 任何 > 0 视为 charging

private:
    // 根据 (m_gear, m_charge) 算候选 view (忽略 hysteresis)
    ViewMode computeCandidate() const;
    // 应用 candidate, 如果距上次切换 >= hysteresis 或 force 立即切
    void tryTransition(uint64_t now_ms, ViewMode candidate, bool force);

    ViewMode m_current       = kDefaultView;
    ViewMode m_pending       = kDefaultView;  // 期望目标 (受 hysteresis 约束)
    // UINT64_MAX sentinel: 永远未切换. 首次切换允许 (free), 后续切换需 hysteresis
    uint64_t m_lastChangeMs  = UINT64_MAX;
    uint64_t m_hysteresisMs  = kDefaultHysteresisMs;
    uint8_t  m_gear          = kGearPark;
    uint8_t  m_charge        = kChargeIdle;
};

}  // namespace candash
