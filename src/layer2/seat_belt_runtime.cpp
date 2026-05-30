// seat_belt_runtime.cpp
#include "seat_belt_runtime.h"
#include "../generated/seat_belt_def.h"
#include <cstdio>

void SeatBeltRuntime::init(const SeatPositionDef* positions, int position_count,
                           const SeatBeltConfigDef* config) {
    m_positions = positions;
    m_config = config;
    m_state.seatCount = position_count;
    m_state.moving = false;
    m_state.wasMoving = false;
    m_state.currentSpeed = 0;
    m_state.speedValid = false;

    for (int i = 0; i < position_count; i++) {
        m_state.seats[i].positionId = positions[i].id;
        m_state.seats[i].seatOccupied = false;
        m_state.seats[i].beltBuckled = false;
        m_state.seats[i].warning = false;
        m_state.seats[i].hint = false;
        m_state.seats[i].lastUpdateMs = 0;
    }
}

void SeatBeltRuntime::updateSpeed(float speed, bool valid) {
    m_state.currentSpeed = speed;
    m_state.speedValid = valid;

    bool was_moving = m_state.moving;
    m_state.moving = valid && (speed > m_config->speed_threshold);

    // 状态切换时重新评估所有座位
    if (was_moving != m_state.moving) {
        for (int i = 0; i < m_state.seatCount; i++) {
            bool occ = m_state.seats[i].seatOccupied;
            bool buck = m_state.seats[i].beltBuckled;
            m_state.seats[i].warning = evaluateSeatWarning(i, occ, buck);
            m_state.seats[i].hint = (!m_state.seats[i].warning && !buck && occ);
        }
    }
}

void SeatBeltRuntime::updateCanFrame(uint32_t can_id, const uint8_t* data, size_t /*len*/) {
    int idx = findSeatByCanId(can_id);
    if (idx < 0) return;

    const SeatPositionDef* pos = &m_positions[idx];
    SeatBeltSeatStatus* seat = &m_state.seats[idx];

    if (pos->seat_occupied_can_id == can_id && pos->seat_occupied_bit < 8) {
        seat->seatOccupied = (data[0] >> pos->seat_occupied_bit) & 1;
    }
    if (pos->buckle_can_id == can_id && pos->buckle_bit < 8) {
        seat->beltBuckled = (data[0] >> pos->buckle_bit) & 1;
    }

    seat->lastUpdateMs = 0;  // TODO: 真实时间戳

    // 重新评估该座位
    seat->warning = evaluateSeatWarning(idx, seat->seatOccupied, seat->beltBuckled);
    seat->hint = (!seat->warning && !seat->beltBuckled && seat->seatOccupied);
}

bool SeatBeltRuntime::evaluateSeatWarning(int seat_idx, bool occupied, bool buckled) const {
    const SeatPositionDef* pos = &m_positions[seat_idx];

    // 规则1：座位无人
    if (m_config->require_seat_occupied && !occupied) return false;
    // 规则2：已系安全带
    if (buckled) return false;
    // 规则3：静止时只 hint 不 warning
    if (!m_state.moving) return false;
    // 规则4：行驶中未系
    return true;
}

int SeatBeltRuntime::findSeatByCanId(uint32_t can_id) const {
    for (int i = 0; i < m_state.seatCount; i++) {
        const SeatPositionDef* pos = &m_positions[i];
        if (pos->seat_occupied_can_id == can_id || pos->buckle_can_id == can_id) {
            return i;
        }
    }
    return -1;
}

void SeatBeltRuntime::tick(uint64_t now_ms) {
    m_lastTickMs = now_ms;
}

void SeatBeltRuntime::query(SeatBeltQueryResult& out) const {
    out.anyWarning = false;
    out.anyUnbuckled = false;
    out.unbuckledCount = 0;

    const char* unbuckled_labels[8];
    int uc = 0;

    for (int i = 0; i < m_state.seatCount; i++) {
        if (m_state.seats[i].warning) out.anyWarning = true;
        if (m_state.seats[i].hint || m_state.seats[i].warning) out.anyUnbuckled = true;
        if (m_state.seats[i].warning) {
            unbuckled_labels[uc++] = m_positions[i].label_zh;
        }
    }
    out.unbuckledCount = uc;

    if (uc == 0) {
        out.messageZh = "";
        out.messageEn = "";
    } else if (uc == 1) {
        snprintf(out.msgBufferZh, sizeof(out.msgBufferZh),
                 "%s请系安全带", unbuckled_labels[0]);
        out.messageZh = out.msgBufferZh;
    } else {
        snprintf(out.msgBufferZh, sizeof(out.msgBufferZh),
                 "%s+%s请系安全带", unbuckled_labels[0], unbuckled_labels[1]);
        out.messageZh = out.msgBufferZh;
    }
}
