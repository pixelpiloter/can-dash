cmake_minimum_required(VERSION 3.20)
project(can-dash VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)  # 禁用 GNU 扩展，确保可移植
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ─── 编译警告（量产项目必须开 -Wall -Wextra -Werror）──────────────────
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
    -Wno-unused-parameter  # TODO: 渐进清理
)

# Release 构建：开 LTO + strip 符号
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O2 -DNDEBUG)
    add_link_options(-flto)
endif()

# Sanitizer 选项：cmake -DCANDASH_SANITIZER=address|thread|undefined
set(CANDASH_SANITIZER "" CACHE STRING "Sanitizer: address, thread, undefined, or empty")
if(CANDASH_SANITIZER)
    add_compile_options(-fsanitize=${CANDASH_SANITIZER} -fno-omit-frame-pointer)
    add_link_options(-fsanitize=${CANDASH_SANITIZER})
    message(STATUS "Sanitizer enabled: ${CANDASH_SANITIZER}")
endif()

# 交叉编译支持：cmake -DCMAKE_TOOLCHAIN_FILE=arm-linux-gnueabihf.cmake
if(CMAKE_CROSSCOMPILING)
    message(STATUS "Cross-compiling for: ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
endif()

# Qt6 — 完全 optional（不装 Qt 也能跑核心 Layer 1/2 测试；装 Qt 才编 UI 二进制 + Layer 3 测试）
find_package(Qt6 QUIET COMPONENTS Core)
if(Qt6_FOUND)
    find_package(Qt6 QUIET COMPONENTS Qml Quick Svg)
endif()
set(QT6_FOUND ${Qt6_FOUND})

set(GENERATED_DIR ${CMAKE_SOURCE_DIR}/src/generated)
set(LAYER1_DIR ${CMAKE_SOURCE_DIR}/src/layer1)
set(LAYER2_DIR ${CMAKE_SOURCE_DIR}/src/layer2)
set(LAYER3_DIR ${CMAKE_SOURCE_DIR}/src/layer3)
set(UI_DIR ${CMAKE_SOURCE_DIR}/src/ui)

# Compile ALL generated C files as C++ to ensure C++ struct definitions are visible
file(GLOB GENERATED_SOURCES CONFIGURE_DEPENDS
    ${GENERATED_DIR}/*.c
    ${GENERATED_DIR}/*.cpp
)
set_source_files_properties(${GENERATED_DIR}/*.c PROPERTIES LANGUAGE CXX)
file(GLOB LAYER2_SOURCES CONFIGURE_DEPENDS ${LAYER2_DIR}/*.cpp ${LAYER2_DIR}/*.h)
# LanguageManager only — DashboardBackend only needs this from layer2
set(LAYER2_LANGUAGE_SOURCES ${LAYER2_DIR}/language_manager.cpp)
file(GLOB LAYER1_SOURCES CONFIGURE_DEPENDS ${LAYER1_DIR}/*.cpp ${LAYER1_DIR}/*.h ${LAYER1_DIR}/shm/*.cpp)
file(GLOB LAYER3_SOURCES CONFIGURE_DEPENDS ${LAYER3_DIR}/*.cpp)

# 编译前自动运行 yaml_to_c.py，然后 post-process 修复 {{ -> {
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    set(FIX_TEMPLATE_SCRIPT ${CMAKE_SOURCE_DIR}/tools/fix_generated.py)
    file(GLOB YAML_CONFIGS ${CMAKE_SOURCE_DIR}/config/*.yaml)
    add_custom_command(OUTPUT ${GENERATED_DIR}/.generated_stamp
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/yaml_to_c.py
        COMMAND ${Python3_EXECUTABLE} ${FIX_TEMPLATE_SCRIPT} ${GENERATED_DIR}
        DEPENDS ${YAML_CONFIGS}
                ${CMAKE_SOURCE_DIR}/tools/yaml_to_c.py
                ${FIX_TEMPLATE_SCRIPT}
        COMMENT "Running yaml_to_c.py and fixing templates"
        VERBATIM)
    add_custom_target(run_yaml_to_c ALL DEPENDS ${GENERATED_DIR}/.generated_stamp)
endif()

# 复制 QML UI 文件到构建目录（仅 Qt6 可用时）
if(Qt6_FOUND)
    file(COPY ${UI_DIR}/ DESTINATION ${CMAKE_BINARY_DIR}/qml/)
    file(COPY ${UI_DIR}/images/ DESTINATION ${CMAKE_BINARY_DIR}/qml/images/)

    # 主应用
    # PR 44 修复: ShmDataSource 引用全部 L2 manager (theme/warning/view/settings/chime/self_test/limp_home + trip),
    #             can-dash 要 link 它们, 不再只 link language_manager
    add_executable(can-dash
        src/main.cpp
        ${LAYER2_SOURCES}
        ${LAYER1_SOURCES}
        ${LAYER3_SOURCES}
        ${GENERATED_SOURCES}
    )
    add_dependencies(can-dash run_yaml_to_c)
    target_include_directories(can-dash PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${GENERATED_DIR}
        ${LAYER1_DIR}
        ${LAYER2_DIR}
        ${LAYER3_DIR}
    )
    target_link_libraries(can-dash PRIVATE Qt6::Core Qt6::Qml Qt6::Quick Qt6::Svg rt)
else()
    message(STATUS "Qt6 not found — skipping can-dash UI binary (UT only)")
endif()

# 单元测试
enable_testing()

# 共享的 Layer 2 测试源（避免重复列表）
set(LAYER2_TEST_SOURCES
    ${LAYER2_DIR}/can_converter.cpp
    ${LAYER2_DIR}/alarm_runtime.cpp
    ${LAYER2_DIR}/indicator_runtime.cpp
    ${LAYER2_DIR}/seat_belt_runtime.cpp
    ${LAYER2_DIR}/limp_home_runtime.cpp  # PR 43
    ${LAYER2_DIR}/can_signal_monitor.cpp
    ${LAYER2_DIR}/event_bus.cpp
    ${LAYER2_DIR}/vehicle_logic.cpp
    ${LAYER2_DIR}/time_util.cpp
)

add_executable(test_can_converter tests/test_can_converter.cpp
    ${LAYER2_TEST_SOURCES} ${GENERATED_SOURCES})
target_include_directories(test_can_converter PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${GENERATED_DIR} ${LAYER1_DIR} ${LAYER2_DIR})
add_dependencies(test_can_converter run_yaml_to_c)
add_test(NAME CanConverterTest COMMAND test_can_converter)

add_executable(test_alarm_runtime tests/test_alarm_runtime.cpp
    ${LAYER2_TEST_SOURCES} ${GENERATED_SOURCES})
target_include_directories(test_alarm_runtime PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${GENERATED_DIR} ${LAYER1_DIR} ${LAYER2_DIR})
add_dependencies(test_alarm_runtime run_yaml_to_c)
add_test(NAME AlarmRuntimeTest COMMAND test_alarm_runtime)

add_executable(test_self_test_runtime tests/test_self_test_runtime.cpp
    ${LAYER2_TEST_SOURCES} ${GENERATED_SOURCES}
    ${LAYER2_DIR}/self_test_runtime.cpp)
target_include_directories(test_self_test_runtime PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${GENERATED_DIR} ${LAYER1_DIR} ${LAYER2_DIR})
add_dependencies(test_self_test_runtime run_yaml_to_c)
add_test(NAME SelfTestRuntimeTest COMMAND test_self_test_runtime)

add_executable(test_seat_belt_runtime tests/test_seat_belt_runtime.cpp
    ${LAYER2_TEST_SOURCES} ${GENERATED_SOURCES})
target_include_directories(test_seat_belt_runtime PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${GENERATED_DIR} ${LAYER1_DIR} ${LAYER2_DIR})
add_dependencies(test_seat_belt_runtime run_yaml_to_c)
add_test(NAME SeatBeltRuntimeTest COMMAND test_seat_belt_runtime)

add_executable(test_limp_home_runtime tests/test_limp_home_runtime.cpp
    ${LAYER2_TEST_SOURCES} ${GENERATED_SOURCES})
target_include_directories(test_limp_home_runtime PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${GENERATED_DIR} ${LAYER1_DIR} ${LAYER2_DIR})
add_dependencies(test_limp_home_runtime run_yaml_to_c)
add_test(NAME LimpHomeRuntimeTest COMMAND test_limp_home_runtime)

add_executable(test_event_bus tests/test_event_bus.cpp
    ${LAYER2_DIR}/event_bus.cpp ${GENERATED_SOURCES})
target_include_directories(test_event_bus PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${GENERATED_DIR} ${LAYER1_DIR} ${LAYER2_DIR})
add_dependencies(test_event_bus run_yaml_to_c)
add_test(NAME EventBusTest COMMAND test_event_bus)

add_executable(test_vehicle_logic tests/test_vehicle_logic.cpp
    ${LAYER2_DIR}/vehicle_logic.cpp
    ${LAYER2_DIR}/event_bus.cpp
    ${LAYER2_DIR}/time_util.cpp
    ${GENERATED_SOURCES})
target_include_directories(test_vehicle_logic PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${GENERATED_DIR} ${LAYER1_DIR} ${LAYER2_DIR})
add_dependencies(test_vehicle_logic run_yaml_to_c)
add_test(NAME VehicleLogicTest COMMAND test_vehicle_logic)

# TripComputer: 派生指标 (距离/均速/时长) — 纯 C++, 无 Qt
add_executable(test_trip_computer tests/test_trip_computer.cpp
    ${LAYER2_DIR}/trip_computer.cpp)
target_include_directories(test_trip_computer PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${LAYER2_DIR})
add_test(NAME TripComputerTest COMMAND test_trip_computer)

# ThemeManager: DAY/NIGHT/AUTO 颜色策略 — 纯 C++, 无 Qt
add_executable(test_theme_manager tests/test_theme_manager.cpp
    ${LAYER2_DIR}/theme_manager.cpp
    ${LAYER2_DIR}/time_util.cpp)  # PR 45: setCurrentHour/reset 调 now_monotonic_ms()
target_include_directories(test_theme_manager PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${LAYER2_DIR})
add_test(NAME ThemeManagerTest COMMAND test_theme_manager)

# WarningManager: 警告聚合/去重/历史 — 纯 C++, 无 Qt
add_executable(test_warning_manager tests/test_warning_manager.cpp
    ${LAYER2_DIR}/warning_manager.cpp)
target_include_directories(test_warning_manager PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${LAYER2_DIR})
add_test(NAME WarningManagerTest COMMAND test_warning_manager)

# SettingsManager: 用户偏好设置 (单位/亮度) — 纯 C++, 无 Qt
add_executable(test_settings_manager tests/test_settings_manager.cpp
    ${LAYER2_DIR}/settings_manager.cpp)
target_include_directories(test_settings_manager PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${LAYER2_DIR})
add_test(NAME SettingsManagerTest COMMAND test_settings_manager)

# ViewManager: 视图模式状态机 (驾驶/充电/驻车) — 纯 C++, 无 Qt
add_executable(test_view_manager tests/test_view_manager.cpp
    ${LAYER2_DIR}/view_manager.cpp)
target_include_directories(test_view_manager PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${LAYER2_DIR})
add_test(NAME ViewManagerTest COMMAND test_view_manager)

# ChimeManager: 声音提示 (severity 映射/防抖/静音/音量) — 纯 C++, 无 Qt
add_executable(test_chime_manager tests/test_chime_manager.cpp
    ${LAYER2_DIR}/chime_manager.cpp)
target_include_directories(test_chime_manager PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${LAYER2_DIR})
add_test(NAME ChimeManagerTest COMMAND test_chime_manager)

# LanguageManager: 多语言 i18n (zh_CN / en_US 翻译表查询) — 纯 C++, 无 Qt
add_executable(test_language_manager tests/test_language_manager.cpp
    ${LAYER2_DIR}/language_manager.cpp)
target_include_directories(test_language_manager PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${LAYER2_DIR})
add_test(NAME LanguageManagerTest COMMAND test_language_manager)

add_executable(test_indicator_runtime tests/test_indicator_runtime.cpp
    ${LAYER2_TEST_SOURCES} ${GENERATED_SOURCES})
target_include_directories(test_indicator_runtime PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${GENERATED_DIR} ${LAYER1_DIR} ${LAYER2_DIR})
add_dependencies(test_indicator_runtime run_yaml_to_c)
add_test(NAME IndicatorRuntimeTest COMMAND test_indicator_runtime)

add_executable(test_can_signal_monitor tests/test_can_signal_monitor.cpp
    ${LAYER2_TEST_SOURCES} ${GENERATED_SOURCES})
target_include_directories(test_can_signal_monitor PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${GENERATED_DIR} ${LAYER1_DIR} ${LAYER2_DIR})
add_dependencies(test_can_signal_monitor run_yaml_to_c)
add_test(NAME CanSignalMonitorTest COMMAND test_can_signal_monitor)

add_executable(test_shm_display tests/test_shm_display.cpp
    ${LAYER1_DIR}/shm/shm_display.cpp)
target_include_directories(test_shm_display PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${LAYER1_DIR} ${LAYER1_DIR}/shm)
add_test(NAME ShmDisplayTest COMMAND test_shm_display)

add_executable(test_time_util tests/test_time_util.cpp
    ${LAYER2_DIR}/time_util.cpp)
target_include_directories(test_time_util PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${LAYER2_DIR})
add_test(NAME TimeUtilTest COMMAND test_time_util)

# LCD 背光超时管理 (PR 51) — 纯 L2 状态机, 无 yaml/L1 依赖
add_executable(test_display_manager tests/test_display_manager.cpp
    ${LAYER2_DIR}/display_manager.cpp)
target_include_directories(test_display_manager PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${LAYER2_DIR})
add_test(NAME DisplayManagerTest COMMAND test_display_manager)

# 性能基线（perf baseline）— 测量数据流热路径耗时
# 永远 print 数字，预算超 50% 时给出 warning（不 hard-fail，避免不同机器抖动）
add_executable(test_perf_baseline tests/test_perf_baseline.cpp
    ${LAYER2_DIR}/alarm_runtime.cpp
    ${LAYER2_DIR}/chime_manager.cpp  # PR 60 perf baseline
    ${LAYER2_DIR}/limp_home_runtime.cpp  # PR 47 perf baseline
    ${LAYER2_DIR}/theme_manager.cpp  # PR 58 perf baseline
    ${LAYER2_DIR}/time_util.cpp
    ${LAYER2_DIR}/trip_computer.cpp  # PR 57 perf baseline
    ${LAYER2_DIR}/warning_manager.cpp  # PR 59 perf baseline
    ${LAYER1_DIR}/shm/shm_display.cpp
    ${GENERATED_SOURCES})
target_include_directories(test_perf_baseline PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${GENERATED_DIR} ${LAYER1_DIR} ${LAYER2_DIR})
add_dependencies(test_perf_baseline run_yaml_to_c)
add_test(NAME PerfBaselineTest COMMAND test_perf_baseline)

# Transport layer tests（sim-socket + socketcan + factory）
add_executable(test_transport tests/test_transport.cpp
    ${LAYER2_DIR}/transport/sim_socket_transport.cpp
    ${LAYER2_DIR}/transport/socketcan_transport.cpp
    ${LAYER2_DIR}/transport/transport_factory.cpp
    ${LAYER2_DIR}/time_util.cpp
    ${LAYER1_DIR}/shm/shm_display.cpp)
target_include_directories(test_transport PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${LAYER1_DIR} ${LAYER2_DIR} ${LAYER2_DIR}/transport)
add_test(NAME TransportTest COMMAND test_transport)

# ─── PR5: Layer 3 抽象层测试（ShmDataSource + QtDataBinder + DashboardBackend 胶水）───
# 仅在 Qt6 可用时编译（依赖 QtCore）
if(Qt6_FOUND)
    # ShmDataSource（Qt + Layer 1 shm）
    # PR 44 修复: ShmDataSource 包含所有 L2 manager (theme/warning/view/settings/chime/self_test/limp_home + trip)
    #            + 全部 GENERATED_SOURCES (signal_table 提供 SIGNAL_TABLE 给 self_test)
    add_executable(test_shm_data_source tests/test_shm_data_source.cpp
        ${LAYER3_DIR}/shm_data_source.cpp
        ${LAYER1_DIR}/shm/shm_display.cpp
        ${LAYER2_DIR}/time_util.cpp
        ${LAYER2_DIR}/trip_computer.cpp
        ${LAYER2_DIR}/theme_manager.cpp
        ${LAYER2_DIR}/warning_manager.cpp
        ${LAYER2_DIR}/view_manager.cpp
        ${LAYER2_DIR}/settings_manager.cpp
        ${LAYER2_DIR}/chime_manager.cpp
        ${LAYER2_DIR}/self_test_runtime.cpp
        ${LAYER2_DIR}/limp_home_runtime.cpp
        ${GENERATED_SOURCES})
    target_include_directories(test_shm_data_source PRIVATE
        ${CMAKE_SOURCE_DIR}/src ${LAYER1_DIR} ${LAYER2_DIR} ${LAYER3_DIR} ${GENERATED_DIR})
    target_link_libraries(test_shm_data_source PRIVATE Qt6::Core)
    add_dependencies(test_shm_data_source run_yaml_to_c)
    add_test(NAME ShmDataSourceTest COMMAND test_shm_data_source)

    # QtDataBinder（Qt + Layer 2 language）
    add_executable(test_qt_data_binder tests/test_qt_data_binder.cpp
        ${LAYER3_DIR}/qt_data_binder.cpp
        ${LAYER2_DIR}/language_manager.cpp)
    target_include_directories(test_qt_data_binder PRIVATE
        ${CMAKE_SOURCE_DIR}/src ${LAYER1_DIR} ${LAYER2_DIR} ${LAYER3_DIR})
    target_link_libraries(test_qt_data_binder PRIVATE Qt6::Core)
    add_test(NAME QtDataBinderTest COMMAND test_qt_data_binder)

    # DashboardBackend 胶水（Qt + Layer 1 shm + Layer 2 language + 所有 Layer 3）
    # PR 44 修复: shm_data_source 引用全部 L2 manager, test_dashboard_backend 也要 link 它们
    add_executable(test_dashboard_backend tests/test_dashboard_backend.cpp
        ${LAYER3_DIR}/dashboard_backend.cpp
        ${LAYER3_DIR}/shm_data_source.cpp
        ${LAYER3_DIR}/qt_data_binder.cpp
        ${LAYER1_DIR}/shm/shm_display.cpp
        ${LAYER2_DIR}/time_util.cpp
        ${LAYER2_DIR}/language_manager.cpp
        ${LAYER2_DIR}/trip_computer.cpp
        ${LAYER2_DIR}/theme_manager.cpp
        ${LAYER2_DIR}/warning_manager.cpp
        ${LAYER2_DIR}/view_manager.cpp
        ${LAYER2_DIR}/settings_manager.cpp
        ${LAYER2_DIR}/chime_manager.cpp
        ${LAYER2_DIR}/self_test_runtime.cpp
        ${LAYER2_DIR}/limp_home_runtime.cpp
        ${GENERATED_SOURCES})
    target_include_directories(test_dashboard_backend PRIVATE
        ${CMAKE_SOURCE_DIR}/src ${LAYER1_DIR} ${LAYER2_DIR} ${LAYER3_DIR} ${GENERATED_DIR})
    target_link_libraries(test_dashboard_backend PRIVATE Qt6::Core)
    add_dependencies(test_dashboard_backend run_yaml_to_c)
    add_test(NAME DashboardBackendTest COMMAND test_dashboard_backend)

    # ChimeManager 数据流集成 (PR 14): ShmDataSource::onTick 桥接 warning→chime
    # PR 44 修复: shm_data_source 引用全部 L2 manager, 跟 test_shm_data_source 同 shape
    add_executable(test_chime_data_flow tests/test_chime_data_flow.cpp
        ${LAYER3_DIR}/shm_data_source.cpp
        ${LAYER1_DIR}/shm/shm_display.cpp
        ${LAYER2_DIR}/time_util.cpp
        ${LAYER2_DIR}/trip_computer.cpp
        ${LAYER2_DIR}/theme_manager.cpp
        ${LAYER2_DIR}/warning_manager.cpp
        ${LAYER2_DIR}/view_manager.cpp
        ${LAYER2_DIR}/settings_manager.cpp
        ${LAYER2_DIR}/chime_manager.cpp
        ${LAYER2_DIR}/self_test_runtime.cpp
        ${LAYER2_DIR}/limp_home_runtime.cpp
        ${GENERATED_SOURCES})
    target_include_directories(test_chime_data_flow PRIVATE
        ${CMAKE_SOURCE_DIR}/src ${LAYER1_DIR} ${LAYER2_DIR} ${LAYER3_DIR} ${GENERATED_DIR})
    target_link_libraries(test_chime_data_flow PRIVATE Qt6::Core)
    add_dependencies(test_chime_data_flow run_yaml_to_c)
    add_test(NAME ChimeDataFlowTest COMMAND test_chime_data_flow)

    # SelfTestRuntime 数据流集成 (PR 17): ShmDataSource::onTick 桥接 signal→self_test
    # PR 44 修复: shm_data_source 引用全部 L2 manager, 跟 test_shm_data_source 同 shape
    add_executable(test_self_test_data_flow tests/test_self_test_data_flow.cpp
        ${LAYER3_DIR}/shm_data_source.cpp
        ${LAYER3_DIR}/qt_data_binder.cpp
        ${LAYER3_DIR}/dashboard_backend.cpp
        ${LAYER1_DIR}/shm/shm_display.cpp
        ${LAYER2_DIR}/time_util.cpp
        ${LAYER2_DIR}/language_manager.cpp
        ${LAYER2_DIR}/trip_computer.cpp
        ${LAYER2_DIR}/theme_manager.cpp
        ${LAYER2_DIR}/warning_manager.cpp
        ${LAYER2_DIR}/view_manager.cpp
        ${LAYER2_DIR}/settings_manager.cpp
        ${LAYER2_DIR}/chime_manager.cpp
        ${LAYER2_DIR}/self_test_runtime.cpp
        ${LAYER2_DIR}/limp_home_runtime.cpp
        ${GENERATED_SOURCES})
    target_include_directories(test_self_test_data_flow PRIVATE
        ${CMAKE_SOURCE_DIR}/src ${LAYER1_DIR} ${LAYER2_DIR} ${LAYER3_DIR} ${GENERATED_DIR})
    target_link_libraries(test_self_test_data_flow PRIVATE Qt6::Core)
    add_dependencies(test_self_test_data_flow run_yaml_to_c)
    add_test(NAME SelfTestDataFlowTest COMMAND test_self_test_data_flow)
endif()
