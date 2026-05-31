#include "shm_display.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

static DisplayDataShm* g_ptr = NULL;
static int g_fd = -1;
static int g_mode = -1;  // 1=creator, 0=opener

static const char* shm_name(void) {
    return SHM_DISPLAY_PATH + 5;  // strip "/dev/"
}

int shm_display_create(void) {
    shm_unlink(shm_name());
    g_fd = shm_open(shm_name(), O_CREAT | O_RDWR, 0666);
    if (g_fd < 0) return -1;
    if (ftruncate(g_fd, sizeof(DisplayDataShm)) < 0) {
        close(g_fd); g_fd = -1; return -1;
    }
    g_ptr = mmap(NULL, sizeof(DisplayDataShm), PROT_READ|PROT_WRITE,
                  MAP_SHARED, g_fd, 0);
    if (g_ptr == MAP_FAILED) {
        g_ptr = NULL; close(g_fd); g_fd = -1; return -1;
    }
    memset(g_ptr, 0, sizeof(DisplayDataShm));
    g_mode = 1;
    return 0;
}

int shm_display_open(void) {
    if (g_ptr) return 0;
    g_fd = shm_open(shm_name(), O_RDWR, 0666);
    if (g_fd < 0) return -1;
    g_ptr = mmap(NULL, sizeof(DisplayDataShm), PROT_READ|PROT_WRITE,
                  MAP_SHARED, g_fd, 0);
    if (g_ptr == MAP_FAILED) {
        g_ptr = NULL; close(g_fd); g_fd = -1; return -1;
    }
    g_mode = 0;
    return 0;
}

void shm_display_write(const DisplayDataShm* data) {
    if (!g_ptr) return;
    memcpy(g_ptr, data, sizeof(DisplayDataShm));
}

static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000ULL;
}

void shm_display_commit(void) {
    if (!g_ptr) return;
    g_ptr->timestamp = get_timestamp_us();
}

void shm_display_mark_updated(ShmFieldIndex idx) {
    if (!g_ptr || idx >= SHM_FIELD_COUNT) return;
    g_ptr->updated_mask |= (1U << idx);
}

void shm_display_set_float(ShmFieldIndex idx, float value) {
    if (!g_ptr) return;
    switch (idx) {
        case SHM_FIELD_MOTOR_RPM:      g_ptr->motor_rpm = value; break;
        case SHM_FIELD_VEHICLE_SPEED:  g_ptr->vehicle_speed = value; break;
        case SHM_FIELD_BAT_VOLT:       g_ptr->bat_volt = value; break;
        case SHM_FIELD_BAT_CURR:       g_ptr->bat_curr = value; break;
        default: return;
    }
    shm_display_mark_updated(idx);
}

void shm_display_set_uint8(ShmFieldIndex idx, uint8_t value) {
    if (!g_ptr) return;
    switch (idx) {
        case SHM_FIELD_BAT_SOC:              g_ptr->bat_soc = value; break;
        case SHM_FIELD_MOTOR_TEMP:           g_ptr->motor_temp = value; break;
        case SHM_FIELD_BRAKE:                g_ptr->brake = value; break;
        case SHM_FIELD_DRIVER_OCCUPIED:      g_ptr->driver_occupied = value; break;
        case SHM_FIELD_PASSENGER_OCCUPIED:   g_ptr->passenger_occupied = value; break;
        case SHM_FIELD_DRIVER_BUCKLED:       g_ptr->driver_buckled = value; break;
        case SHM_FIELD_PASSENGER_BUCKLED:    g_ptr->passenger_buckled = value; break;
        case SHM_FIELD_REAR_BUCKLE:          g_ptr->rear_buckle = value; break;
        default: return;
    }
    shm_display_mark_updated(idx);
}

void shm_display_set_alarm(const char* msg_zh) {
    if (!g_ptr) return;
    g_ptr->alarm_active = (msg_zh && msg_zh[0]) ? 1 : 0;
    if (msg_zh) {
        strncpy(g_ptr->alarm_message_zh, msg_zh, SHM_ALARM_TEXT_LEN - 1);
        g_ptr->alarm_message_zh[SHM_ALARM_TEXT_LEN - 1] = '\0';
    } else {
        g_ptr->alarm_message_zh[0] = '\0';
    }
}

void shm_display_set_indicator(ShmIndicatorId id, int on, int flash, float hz) {
    if (!g_ptr || id >= SHM_INDICATOR_COUNT) return;
    g_ptr->indicators[id].on = on ? 1 : 0;
    g_ptr->indicators[id].flash = flash ? 1 : 0;
    g_ptr->indicators[id].hz_x10 = (uint8_t)(hz * 10 + 0.5f);
}

uint64_t shm_display_read(DisplayDataShm* out_data) {
    if (!g_ptr) return 0;
    if (out_data) memcpy(out_data, g_ptr, sizeof(DisplayDataShm));
    return g_ptr->timestamp;
}

uint64_t shm_display_poll(uint64_t last_timestamp) {
    if (!g_ptr) return last_timestamp;
    return g_ptr->timestamp;
}

void shm_display_close(void) {
    if (g_ptr && g_ptr != MAP_FAILED) {
        munmap(g_ptr, sizeof(DisplayDataShm));
        g_ptr = NULL;
    }
    if (g_fd >= 0) {
        close(g_fd);
        g_fd = -1;
    }
}
