// shm_display.cpp
// 使用 /dev/shm/can_display 路径 + 常规文件 open() + mmap
#include "shm_display.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>

static int g_fd = -1;
static DisplayDataShm* g_ptr = NULL;

// ─── 创建（processor用）────────────────────────────────
int shm_display_create(void) {
    unlink(SHM_DISPLAY_PATH);
    g_fd = open(SHM_DISPLAY_PATH, O_RDWR | O_CREAT | O_EXCL, 0664);
    if (g_fd < 0) { fprintf(stderr, "[shm] create: %s\n", strerror(errno)); return -1; }
    if (ftruncate(g_fd, sizeof(DisplayDataShm)) < 0) {
        fprintf(stderr, "[shm] ftruncate: %s\n", strerror(errno));
        close(g_fd); unlink(SHM_DISPLAY_PATH); g_fd=-1; return -1;
    }
    g_ptr = (DisplayDataShm*)mmap(NULL, sizeof(DisplayDataShm),
                                   PROT_READ|PROT_WRITE, MAP_SHARED, g_fd, 0);
    if (g_ptr == MAP_FAILED) {
        fprintf(stderr, "[shm] mmap: %s\n", strerror(errno));
        close(g_fd); unlink(SHM_DISPLAY_PATH); g_fd=-1; return -1;
    }
    memset(g_ptr, 0, sizeof(DisplayDataShm));
    return 0;
}

// ─── 打开（dash用）─────────────────────────────────────
int shm_display_open(void) {
    g_fd = open(SHM_DISPLAY_PATH, O_RDONLY);
    if (g_fd < 0) return -1;
    g_ptr = (DisplayDataShm*)mmap(NULL, sizeof(DisplayDataShm),
                                   PROT_READ, MAP_SHARED, g_fd, 0);
    if (g_ptr == MAP_FAILED) { close(g_fd); g_fd=-1; return -1; }
    return 0;
}

// ─── 轮询 ──────────────────────────────────────────────
uint64_t shm_display_poll(uint64_t last_ts) {
    if (!g_ptr) return last_ts;
    return g_ptr->timestamp;
}

// ─── 读（dash用）───────────────────────────────────────
uint64_t shm_display_read(DisplayDataShm* out) {
    if (!g_ptr || !out) return 0;
    memcpy(out, g_ptr, sizeof(DisplayDataShm));
    return g_ptr->timestamp;
}

// ─── 写整个结构（processor用）──────────────────────────
void shm_display_write(const DisplayDataShm* data) {
    if (!g_ptr || !data) return;
    memcpy(g_ptr, data, sizeof(DisplayDataShm));
}

// ─── 标记字段已更新 ────────────────────────────────────
void shm_display_mark_updated(ShmFieldIndex idx) {
    if (!g_ptr || idx >= SHM_FIELD_COUNT) return;
    g_ptr->updated_mask |= (1U << idx);
}

// ─── 字段写入 ─────────────────────────────────────────
void shm_display_set_float(ShmFieldIndex idx, float value) {
    if (!g_ptr || idx >= SHM_FIELD_COUNT) return;
    switch (idx) {
        case SHM_FIELD_BAT_VOLT:      g_ptr->bat_volt = value; break;
        case SHM_FIELD_BAT_CURR:     g_ptr->bat_curr = value; break;
        case SHM_FIELD_VEHICLE_SPEED: g_ptr->vehicle_speed = value; break;
        case SHM_FIELD_MOTOR_RPM:    g_ptr->motor_rpm = value; break;
        default: return;
    }
    shm_display_mark_updated(idx);
}

void shm_display_set_uint8(ShmFieldIndex idx, uint8_t value) {
    if (!g_ptr || idx >= SHM_FIELD_COUNT) return;
    switch (idx) {
        case SHM_FIELD_BAT_SOC:             g_ptr->bat_soc = value; break;
        case SHM_FIELD_MOTOR_TEMP:          g_ptr->motor_temp = value; break;
        case SHM_FIELD_BRAKE:               g_ptr->brake = value; break;
        case SHM_FIELD_DRIVER_OCCUPIED:     g_ptr->driver_occupied = value; break;
        case SHM_FIELD_PASSENGER_OCCUPIED:  g_ptr->passenger_occupied = value; break;
        case SHM_FIELD_DRIVER_BUCKLED:      g_ptr->driver_buckled = value; break;
        case SHM_FIELD_PASSENGER_BUCKLED:   g_ptr->passenger_buckled = value; break;
        case SHM_FIELD_REAR_BUCKLE:         g_ptr->rear_buckle = value; break;
        default: return;
    }
    shm_display_mark_updated(idx);
}

void shm_display_set_indicator(ShmIndicatorId id, int on, int flash, float hz) {
    if (!g_ptr || id < 0 || id >= SHM_INDICATOR_COUNT) return;
    g_ptr->indicators[id].on = (uint8_t)on;
    g_ptr->indicators[id].flash = (uint8_t)flash;
    g_ptr->indicators[id].hz_x10 = (uint8_t)(hz * 10 + 0.5f);
}

void shm_display_set_alarm(const char* text_zh) {
    if (!g_ptr) return;
    if (text_zh && text_zh[0]) {
        g_ptr->alarm_active = 1;
        strncpy(g_ptr->alarm_message_zh, text_zh, SHM_ALARM_TEXT_LEN - 1);
        g_ptr->alarm_message_zh[SHM_ALARM_TEXT_LEN - 1] = '\0';
    } else {
        g_ptr->alarm_active = 0;
        g_ptr->alarm_message_zh[0] = '\0';
    }
}

// ─── 提交（递增timestamp，sync）────────────────────────
void shm_display_commit(void) {
    if (!g_ptr) return;
    g_ptr->timestamp++;
    msync(g_ptr, sizeof(DisplayDataShm), MS_SYNC);
}

// ─── 关闭 ──────────────────────────────────────────────
void shm_display_close(void) {
    if (g_ptr && g_ptr != (void*)MAP_FAILED) munmap(g_ptr, sizeof(DisplayDataShm));
    if (g_fd >= 0) close(g_fd);
    g_ptr = NULL; g_fd = -1;
}
