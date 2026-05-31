cmake_minimum_required(VERSION 3.16)
project(can-processor CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CAN_DASH_ROOT /home/cjl/can-dash)
set(GENERATED_DIR ${CAN_DASH_ROOT}/src/generated)

include_directories(
    ${CAN_DASH_ROOT}/src
    ${GENERATED_DIR}
)

set(SHM_SOURCES
    ${CAN_DASH_ROOT}/src/layer1/shm/shm_display.cpp
)

set(L2_SOURCES
    ${CAN_DASH_ROOT}/src/layer2/can_converter.cpp
    ${CAN_DASH_ROOT}/src/layer2/alarm_runtime.cpp
    ${CAN_DASH_ROOT}/src/layer2/indicator_runtime.cpp
    ${CAN_DASH_ROOT}/src/layer2/seat_belt_runtime.cpp
)

set(GENERATED_SOURCES
    ${GENERATED_DIR}/can_field_table.cpp
    ${GENERATED_DIR}/alarm_rule_table.cpp
    ${GENERATED_DIR}/indicator_table.cpp
    ${GENERATED_DIR}/seat_belt_table.cpp
    ${GENERATED_DIR}/signal_table.cpp
)

add_executable(can-processor
    main.cpp
    ${SHM_SOURCES}
    ${L2_SOURCES}
    ${GENERATED_SOURCES}
)

target_link_libraries(can-processor pthread rt)
