// seat_belt_runtime.h
// Layer 2: 安全带业务逻辑
// 纯 C++，无 Qt，无 YAML 运行时

#pragma once
#include <cstdint>
#include <cstring>

struct SeatPositionDef;
struct SeatBeltConfigDef;

// 座位状态
struct SeatBeltSeatStatus {
    const char*       positionId;
    bool              seatOccupied;
    bool              beltBuckled;
    bool              warning;     // 行驶中未系（报警）
    bool              hint;        // 静止时未系（提示）
    uint64_t          lastUpdateMs;
};

// 运行时状态
struct SeatBeltRuntimeState {
    bool       moving;        // 当前是否行驶中
    bool       wasMoving;      // 上个状态是否行驶中
    float      currentSpeed;
    bool       speedValid;
    int        seatCount;
    SeatBeltSeatStatus seats[8];  // 最多8个座位
};

// 查询结果
struct SeatBeltQueryResult {
    bool        anyWarning;     // 有任何座位报警
    bool        anyUnbuckled;   // 有任何座位未系
    int         unbuckledCount;
    const char* messageZh;      // 静态指针
    const char* messageEn;
    char        msgBufferZh[128];
    char        msgBufferEn[128];
};

class SeatBeltRuntime {
public:
    SeatBeltRuntime() = default;

    void init(const SeatPositionDef* positions, int position_count,
              const SeatBeltConfigDef* config);

    // 车速更新（来自 EventBus）
    void updateSpeed(float speed, bool valid);

    // CAN 帧更新（来自 EventBus）
    void updateCanFrame(uint32_t can_id, const uint8_t* data, size_t len);

    // 定时调用
    void tick(uint64_t now_ms);

    // 查询状态
    void query(SeatBeltQueryResult& out) const;

    // 获取内部状态（供 Layer 3 使用）
    const SeatBeltRuntimeState& states() const { return m_state; }

    const char* name() const { return "SeatBeltRuntime"; }

private:
    bool evaluateSeatWarning(int seat_idx, bool occupied, bool buckled) const;
    int  findSeatByCanId(uint32_t can_id) const;

    const SeatPositionDef* m_positions = nullptr;
    const SeatBeltConfigDef* m_config = nullptr;
    SeatBeltRuntimeState m_state = {};
    uint64_t m_lastTickMs = 0;
};
