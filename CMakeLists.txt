cmake_minimum_required(VERSION 3.20)
project(can-dash VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Qml Quick Svg)

set(GENERATED_DIR ${CMAKE_SOURCE_DIR}/src/generated)
set(LAYER1_DIR ${CMAKE_SOURCE_DIR}/src/layer1)
set(LAYER2_DIR ${CMAKE_SOURCE_DIR}/src/layer2)
set(LAYER3_DIR ${CMAKE_SOURCE_DIR}/src/layer3)
set(UI_DIR ${CMAKE_SOURCE_DIR}/src/ui)

file(GLOB GENERATED_SOURCES ${GENERATED_DIR}/*.c ${GENERATED_DIR}/*.cpp ${GENERATED_DIR}/*.h)
file(GLOB LAYER2_SOURCES ${LAYER2_DIR}/*.cpp ${LAYER2_DIR}/*.h)

# 主应用
add_executable(can-dash
    src/main.cpp
    ${LAYER1_DIR}/*.h
    ${LAYER2_SOURCES}
    ${LAYER3_DIR}/*.cpp ${LAYER3_DIR}/*.h
    ${GENERATED_SOURCES}
)

target_include_directories(can-dash PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${GENERATED_DIR}
    ${LAYER1_DIR}
    ${LAYER2_DIR}
    ${LAYER3_DIR}
)

target_link_libraries(can-dash PRIVATE Qt6::Core Qt6::Qml Qt6::Quick Qt6::Svg)

# 编译前自动运行 yaml_to_c.py
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_custom_command(OUTPUT ${GENERATED_DIR}/.generated_stamp
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/yaml_to_c.py
        DEPENDS ${CMAKE_SOURCE_DIR}/config/*.yaml ${CMAKE_SOURCE_DIR}/tools/yaml_to_c.py
        COMMENT "Running yaml_to_c.py"
        VERBATIM)
    add_custom_target(run_yaml_to_c ALL DEPENDS ${GENERATED_DIR}/.generated_stamp)
    add_dependencies(can-dash run_yaml_to_c)
endif()

# 单元测试
enable_testing()
add_executable(test_can_converter tests/test_can_converter.cpp
    ${LAYER1_DIR}/*.h ${LAYER2_DIR}/can_converter.cpp ${GENERATED_SOURCES})
add_test(NAME CanConverterTest COMMAND test_can_converter)

add_executable(test_alarm_runtime tests/test_alarm_runtime.cpp
    ${LAYER1_DIR}/*.h ${LAYER2_DIR}/alarm_runtime.cpp ${GENERATED_SOURCES})
add_test(NAME AlarmRuntimeTest COMMAND test_alarm_runtime)

add_executable(test_seat_belt_runtime tests/test_seat_belt_runtime.cpp
    ${LAYER1_DIR}/*.h ${LAYER2_DIR}/seat_belt_runtime.cpp ${GENERATED_SOURCES})
add_test(NAME SeatBeltRuntimeTest COMMAND test_seat_belt_runtime)
