cmake_minimum_required(VERSION 3.20)
project(can-dash VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt6 — 找不到时不 abort，只跳过 UI binary
find_package(Qt6 COMPONENTS Core Qml Quick Svg)
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
    add_custom_command(OUTPUT ${GENERATED_DIR}/.generated_stamp
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/yaml_to_c.py
        COMMAND ${Python3_EXECUTABLE} ${FIX_TEMPLATE_SCRIPT} ${GENERATED_DIR}
        DEPENDS ${CMAKE_SOURCE_DIR}/config/*.yaml
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
    add_executable(can-dash
        src/main.cpp
        ${LAYER2_LANGUAGE_SOURCES}
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
add_executable(test_can_converter tests/test_can_converter.cpp
    ${LAYER2_DIR}/can_converter.cpp ${GENERATED_SOURCES})
target_include_directories(test_can_converter PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${GENERATED_DIR} ${LAYER1_DIR} ${LAYER2_DIR})
add_dependencies(test_can_converter run_yaml_to_c)
add_test(NAME CanConverterTest COMMAND test_can_converter)

add_executable(test_alarm_runtime tests/test_alarm_runtime.cpp
    ${LAYER2_DIR}/alarm_runtime.cpp ${GENERATED_SOURCES})
target_include_directories(test_alarm_runtime PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${GENERATED_DIR} ${LAYER1_DIR} ${LAYER2_DIR})
add_dependencies(test_alarm_runtime run_yaml_to_c)
add_test(NAME AlarmRuntimeTest COMMAND test_alarm_runtime)

add_executable(test_seat_belt_runtime tests/test_seat_belt_runtime.cpp
    ${LAYER2_DIR}/seat_belt_runtime.cpp ${GENERATED_SOURCES})
target_include_directories(test_seat_belt_runtime PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${GENERATED_DIR} ${LAYER1_DIR} ${LAYER2_DIR})
add_dependencies(test_seat_belt_runtime run_yaml_to_c)
add_test(NAME SeatBeltRuntimeTest COMMAND test_seat_belt_runtime)

add_executable(test_event_bus tests/test_event_bus.cpp
    ${LAYER2_DIR}/event_bus.cpp ${GENERATED_SOURCES})
target_include_directories(test_event_bus PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${GENERATED_DIR} ${LAYER1_DIR} ${LAYER2_DIR})
add_dependencies(test_event_bus run_yaml_to_c)
add_test(NAME EventBusTest COMMAND test_event_bus)

add_executable(test_vehicle_logic tests/test_vehicle_logic.cpp
    ${LAYER2_DIR}/vehicle_logic.cpp
    ${LAYER2_DIR}/event_bus.cpp
    ${GENERATED_SOURCES})
target_include_directories(test_vehicle_logic PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${GENERATED_DIR} ${LAYER1_DIR} ${LAYER2_DIR})
add_dependencies(test_vehicle_logic run_yaml_to_c)
add_test(NAME VehicleLogicTest COMMAND test_vehicle_logic)

add_executable(test_indicator_runtime tests/test_indicator_runtime.cpp
    ${LAYER2_DIR}/indicator_runtime.cpp ${GENERATED_SOURCES})
target_include_directories(test_indicator_runtime PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${GENERATED_DIR} ${LAYER1_DIR} ${LAYER2_DIR})
add_dependencies(test_indicator_runtime run_yaml_to_c)
add_test(NAME IndicatorRuntimeTest COMMAND test_indicator_runtime)

add_executable(test_can_signal_monitor tests/test_can_signal_monitor.cpp
    ${LAYER2_DIR}/can_signal_monitor.cpp ${GENERATED_SOURCES})
target_include_directories(test_can_signal_monitor PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${GENERATED_DIR} ${LAYER1_DIR} ${LAYER2_DIR})
add_dependencies(test_can_signal_monitor run_yaml_to_c)
add_test(NAME CanSignalMonitorTest COMMAND test_can_signal_monitor)
