// test_shm_display.cpp
// Layer 1 共享内存层单元测试：magic/version 校验 + CRC32 checksum + 心跳

// 这个测试大量依赖 assert() 实际生效（abort-on-fail）。
// Release 构建会 -DNDEBUG 把 assert 变 no-op，导致错误流程被吞掉、测试假绿。
// 注意：必须先 #undef NDEBUG 再 #include <cassert>，否则 cassert 已经按
// NDEBUG 状态定义了一个 no-op 版本，undef 也救不回来。
#undef NDEBUG
#include <cstdio>
#include <cstring>
#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "layer1/shm/shm_display.h"

#define TEST_SHM_PATH "/tmp/candash_test_shm"

static void set_test_env() {
    setenv("CANDASH_SHM_PATH", TEST_SHM_PATH, 1);
    setenv("CANDASH_SOCKET_PATH", "/tmp/candash_test_sock", 1);
}

static void cleanup_shm() {
    unlink(TEST_SHM_PATH);
}

// 翻转指定偏移的 1 个 bit（用于 fault injection）
static void flip_bit_in_shm(size_t offset) {
    int fd = open(TEST_SHM_PATH, O_RDWR);
    if (fd < 0) { perror("flip_bit: open"); abort(); }
    uint8_t* raw = (uint8_t*)mmap(NULL, sizeof(DisplayDataShm),
                                  PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (raw == MAP_FAILED) { perror("flip_bit: mmap"); abort(); }
    raw[offset] ^= 0x01;
    msync(raw, sizeof(DisplayDataShm), MS_SYNC);
    munmap(raw, sizeof(DisplayDataShm));
    close(fd);
}

int main() {
    printf("=== shm_display 单元测试 ===\n");
    set_test_env();
    cleanup_shm();

    // ─── 测试 1：ABI 布局（488 字节）───
    printf("\n[测试1] DisplayDataShm 布局/大小\n");
    assert(sizeof(DisplayDataShm) == 488);
    assert(offsetof(DisplayDataShm, magic) == 0);
    assert(offsetof(DisplayDataShm, version) == 4);
    assert(offsetof(DisplayDataShm, last_commit_ms) == 8);
    assert(offsetof(DisplayDataShm, updated_mask) == 16);
    assert(offsetof(DisplayDataShm, checksum) == 20);
    printf("  ✓ 字段偏移正确，大小 = 488 字节\n");

    // ─── 测试 2：magic / version 常量 ───
    printf("\n[测试2] ABI 标识常量\n");
    assert(SHM_MAGIC == 0xCA07D15A);
    assert(SHM_VERSION_MAJOR == 1);
    assert(SHM_VERSION_MINOR == 1);
    assert(SHM_VERSION_PATCH == 0);
    assert(SHM_VERSION == 10100);
    printf("  ✓ magic=0x%08X version=%u\n", SHM_MAGIC, SHM_VERSION);

    // ─── 测试 3：processor 写 → dash 读（happy path）───
    printf("\n[测试3] processor 写 → dash 读（checksum 校验）\n");
    cleanup_shm();
    int rc_c3 = shm_display_create();
    fprintf(stderr, "  [dbg3] create=%d\n", rc_c3);
    assert(rc_c3 == 0);
    shm_display_set_float(SHM_FIELD_VEHICLE_SPEED, 88.5f);
    shm_display_set_uint8(SHM_FIELD_BAT_SOC, 73);
    shm_display_set_uint16(SHM_FIELD_EV_RANGE, 320);
    fprintf(stderr, "  [dbg3] before commit: speed=%.2f soc=%u ev=%u magic=0x%X ver=%u\n",
        shm_display_health_check() == 0 ? 88.5f : 0.0f, 73, 320,
        SHM_MAGIC, SHM_VERSION);
    shm_display_commit();
    // 读自己刚写的 mmap 区域（g_ptr 还是 mmap 区域）
    fprintf(stderr, "  [dbg3] after commit (before close)\n");
    shm_display_close();
    fprintf(stderr, "  [dbg3] closed, file=%s\n",
        access(TEST_SHM_PATH, F_OK) == 0 ? "exists" : "missing");
    // 检查文件大小
    struct stat st;
    if (stat(TEST_SHM_PATH, &st) == 0) {
        fprintf(stderr, "  [dbg3] file size=%lld\n", (long long)st.st_size);
    }

    assert(shm_display_open() == 0);
    assert(shm_display_health_check() == 0);
    DisplayDataShm data = {};
    uint64_t commit_ms = 0;
    int rc_read = shm_display_read(&data, &commit_ms);
    fprintf(stderr, "  [dbg3] rc_read=%d, read data: speed=%.2f soc=%u ev=%u magic=0x%X ver=%u commit_ms=%lu checksum=0x%X\n",
        rc_read, data.vehicle_speed, data.bat_soc, data.ev_range,
        data.magic, data.version, (unsigned long)commit_ms, data.checksum);
    assert(rc_read == 0);
    assert(data.vehicle_speed > 88.0f && data.vehicle_speed < 89.0f);
    assert(data.bat_soc == 73);
    assert(data.ev_range == 320);
    assert(commit_ms > 0);
    printf("  ✓ 数据一致：speed=%.1f soc=%u ev_range=%u commit_ms=%lu\n",
           data.vehicle_speed, data.bat_soc, data.ev_range,
           (unsigned long)commit_ms);
    shm_display_close();
    cleanup_shm();

    // ─── 测试 4：checksum 损坏检测（fault injection）───
    printf("\n[测试4] checksum 损坏检测（bit flip 故障注入）\n");
    assert(shm_display_create() == 0);
    shm_display_set_float(SHM_FIELD_MOTOR_RPM, 1500.0f);
    shm_display_commit();
    shm_display_close();

    // 翻 motor_rpm 区域一个 bit → 校验和不匹配
    flip_bit_in_shm(100);  // offset 100 落在 alarm_msg 或 _padding 区

    assert(shm_display_open() == 0);
    DisplayDataShm bad = {};
    int bad_rc = shm_display_read(&bad, nullptr);
    assert(bad_rc == -3);
    printf("  ✓ checksum 损坏时返回 -3\n");
    shm_display_close();
    cleanup_shm();

    // ─── 测试 5：magic 不匹配（ABI 错乱保护）───
    printf("\n[测试5] magic 不匹配保护\n");
    {
        int fd = open(TEST_SHM_PATH, O_RDWR|O_CREAT|O_EXCL, 0664);
        assert(fd >= 0);
        int ft_rc = ftruncate(fd, sizeof(DisplayDataShm));
        assert(ft_rc == 0);
        DisplayDataShm bad_data = {};
        bad_data.magic = 0xDEADBEEF;  // 错误的 magic
        bad_data.version = SHM_VERSION;
        ssize_t wr = write(fd, &bad_data, sizeof(DisplayDataShm));
        assert(wr == (ssize_t)sizeof(DisplayDataShm));
        close(fd);
    }
    int open_rc = shm_display_open();
    assert(open_rc == -2);
    assert(shm_display_health_check() == -1);
    printf("  ✓ 错误 magic 时 open 返回 -2\n");
    cleanup_shm();

    // ─── 测试 6：age_ms 心跳（stale 检测）───
    printf("\n[测试6] 心跳 age_ms（stale 检测）\n");
    assert(shm_display_create() == 0);
    shm_display_commit();
    shm_display_close();

    assert(shm_display_open() == 0);
    uint64_t fake_now = 0;
    DisplayDataShm tmp = {};
    shm_display_read(&tmp, &fake_now);
    assert(fake_now > 0);
    assert(shm_display_age_ms(fake_now + 1000) == 1000);
    assert(shm_display_age_ms(fake_now) == 0);
    assert(shm_display_age_ms(fake_now - 100) == 0);
    printf("  ✓ age_ms 正常路径 OK\n");
    shm_display_close();
    cleanup_shm();

    // ─── 测试 7：path 环境变量覆盖 ───
    printf("\n[测试7] 路径环境变量覆盖\n");
    setenv("CANDASH_SHM_PATH", "/tmp/candash_custom_path", 1);
    assert(strcmp(shm_display_get_path(), "/tmp/candash_custom_path") == 0);
    unsetenv("CANDASH_SHM_PATH");
    const char* def = shm_display_get_path();
    assert(def != nullptr && def[0] == '/');
    printf("  ✓ 默认路径=%s\n", def);
    cleanup_shm();

    printf("\n所有测试通过。\n");
    return 0;
}
