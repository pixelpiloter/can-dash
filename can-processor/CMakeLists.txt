cmake_minimum_required(VERSION 3.16)
project(can-processor CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── can-dash 根路径解析（构建可重现，禁止硬编码绝对路径）──────────
# 优先级：环境变量 CAN_DASH_ROOT > 父目录自动检测
if(DEFINED ENV{CAN_DASH_ROOT})
    set(CAN_DASH_ROOT "$ENV{CAN_DASH_ROOT}")
    message(STATUS "can-dash root (from env): ${CAN_DASH_ROOT}")
else()
    set(CAN_DASH_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
    get_filename_component(CAN_DASH_ROOT ${CAN_DASH_ROOT} ABSOLUTE)
    message(STATUS "can-dash root (auto): ${CAN_DASH_ROOT}")
endif()

# 校验根目录存在，防止 CI 在错误目录 build
if(NOT EXISTS "${CAN_DASH_ROOT}/src/layer2/can_converter.cpp")
    message(FATAL_ERROR
        "CAN_DASH_ROOT='${CAN_DASH_ROOT}' 不是一个有效的 can-dash 仓库根。\n"
        "请设置环境变量 CAN_DASH_ROOT 指向包含 src/、config/ 的目录。")
endif()

set(GENERATED_DIR ${CAN_DASH_ROOT}/src/generated)

include_directories(
    ${CAN_DASH_ROOT}/src
    ${CAN_DASH_ROOT}/src/layer2
    ${GENERATED_DIR}
)

# ── 源文件清单（明确列出，禁止 GLOB，保证构建可重现）──────────
# Layer 1: 共享内存
set(SHM_SOURCES
    ${CAN_DASH_ROOT}/src/layer1/shm/shm_display.cpp
)

# Layer 2: 纯 C++ 业务逻辑
# （列出全部——只用 main.cpp 间接引用部分，但显式列表便于审计）
set(L2_SOURCES
    ${CAN_DASH_ROOT}/src/layer2/can_converter.cpp
    ${CAN_DASH_ROOT}/src/layer2/alarm_runtime.cpp
    ${CAN_DASH_ROOT}/src/layer2/can_signal_monitor.cpp
    ${CAN_DASH_ROOT}/src/layer2/event_bus.cpp
    ${CAN_DASH_ROOT}/src/layer2/indicator_runtime.cpp
    ${CAN_DASH_ROOT}/src/layer2/seat_belt_runtime.cpp
    ${CAN_DASH_ROOT}/src/layer2/time_util.cpp
    ${CAN_DASH_ROOT}/src/layer2/vehicle_logic.cpp
    # CAN 传输层抽象（sim-socket + socketcan）
    ${CAN_DASH_ROOT}/src/layer2/transport/sim_socket_transport.cpp
    ${CAN_DASH_ROOT}/src/layer2/transport/socketcan_transport.cpp
    ${CAN_DASH_ROOT}/src/layer2/transport/transport_factory.cpp
)

# Layer 1 生成代码（YAML → C 预处理产物）
set(GENERATED_SOURCES
    ${GENERATED_DIR}/can_field_table.cpp
    ${GENERATED_DIR}/alarm_rule_table.cpp
    ${GENERATED_DIR}/indicator_table.cpp
    ${GENERATED_DIR}/limp_home_table.cpp
    ${GENERATED_DIR}/seat_belt_table.cpp
    ${GENERATED_DIR}/signal_table.cpp
    ${GENERATED_DIR}/vehicle_config.cpp
)

add_executable(can-processor
    main.cpp
    ${SHM_SOURCES}
    ${L2_SOURCES}
    ${GENERATED_SOURCES}
)

# ── 编译警告（车规：必须 0 warning，CI 把警告当错误）──────────────────
#  对齐顶级 CMakeLists.txt 的严格级别，外加 -Werror 强制 0 警告。
add_compile_options(
    -Wall
    -Wextra
    -Wshadow
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Wcast-align
    -Wunused
    -Woverloaded-virtual
    -Wconversion
    -Wsign-conversion
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
    -Werror
    -Wno-unused-parameter  # TODO: 渐进清理
)

# pthread: shm + 未来多线程
# rt: clock_gettime
target_link_libraries(can-processor pthread rt)
