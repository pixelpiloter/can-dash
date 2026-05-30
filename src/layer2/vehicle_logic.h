// vehicle_logic.h
// Layer 2: 车速/SOC/驾驶模式业务逻辑

#pragma once
#include <cstdint>
#include <cstring>
#include <array>

struct VehicleConfigDef {
    float  soc_warning_low = 10.0f;
    float  soc_critical_low = 5.0f;
    float  speed_max = 260.0f;
    uint32_t precharge_timeout_ms = 3000;
    int    soc_smoothing_window = 5;
};

enum DriveMode { DRIVE_MODE_ECO, DRIVE_MODE_NORMAL, DRIVE_MODE_SPORT };
enum PrechargeState { PRECHARGE_IDLE, PRECHARGE_ACTIVE, PRECHARGE_DONE, PRECHARGE_FAILED };

class VehicleLogic {
public:
    VehicleLogic();

    void init(const VehicleConfigDef* config);

    // 输入
    void onSpeedUpdate(float speed, bool valid);
    void onSocUpdate(float soc);
    void onHvStatusUpdate(bool active);

    // 定时调用
    void tick(uint64_t now_ms);

    // 查询
    float getSpeed() const { return m_speed; }
    bool  isSpeedValid() const { return m_speedValid; }
    float getSmoothedSoc() const;
    bool  isSocLow() const;
    bool  isSocCritical() const;
    bool  isReadyGo() const;
    bool  isHvActive() const { return m_hvActive; }
    PrechargeState getPrechargeState() const { return m_prechargeState; }
    const char* getDriveModeStr() const;

    const char* name() const { return "VehicleLogic"; }

private:
    VehicleConfigDef  m_config;
    float             m_speed;
    float             m_lastSpeed;
    bool              m_speedValid;
    float             m_targetSpeed;
    float             m_soc;
    float             m_socSmoothed;
    std::array<float, 10> m_socHistory;
    int               m_socHistoryIndex;
    DriveMode         m_driveMode;
    PrechargeState    m_prechargeState;
    uint64_t          m_prechargeStartMs;
    bool              m_readyGoActive;
    bool              m_hvActive;
    uint64_t          m_lastSpeedUpdateMs;
    uint64_t          m_lastTickMs;
};
