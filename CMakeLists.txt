# projectile -- the servo launcher (app/main.cpp) plus every ENG1104
# Week 8-10 exercise, in one CMake project
#
#   app/  projectile -- THE project program; all project code is in main.cpp
#   w08/  GPIO, sensor programming & firmware debugging   (5 programs)
#   w09/  Timers, PWM actuation & data collection          (8 programs)
#   w10/  IMU C++ class                                     (1 program)
#
# The course files (board.h, common/, w08..w10) are as provided; one
# board.h and one common/ (the newest Week 10 drivers) serve all weeks.
# Every program is still its own .uf2 -- pick the one to build in the CMake panel,
# or build "all".
#
# Turn weeks off to speed up configure/build, e.g.  -DENG1104_BUILD_W08=OFF

cmake_minimum_required(VERSION 3.13)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

set(PICO_BOARD pico_w CACHE STRING "Board type")
include(pico_sdk_import.cmake)

project(projectile C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

pico_sdk_init()

option(PROJECTILE_BUILD_APP "Build the projectile program" ON)
option(ENG1104_BUILD_W08 "Build the Week 8 exercises"  ON)
option(ENG1104_BUILD_W09 "Build the Week 9 exercises"  ON)
option(ENG1104_BUILD_W10 "Build the Week 10 exercises" ON)

# --- shared pin map --------------------------------------------------------
add_library(eng1104_board INTERFACE)
target_include_directories(eng1104_board INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(eng1104_board INTERFACE pico_stdlib)

set(ENG1104_COMMON ${CMAKE_CURRENT_LIST_DIR}/common)

# --- shared C drivers (C++ code reaches them through extern "C" guards) -----

add_library(gy511 STATIC ${ENG1104_COMMON}/gy511.c)
target_include_directories(gy511 PUBLIC ${ENG1104_COMMON})
target_link_libraries(gy511 PUBLIC pico_stdlib hardware_i2c)

add_library(lsm6ds3 STATIC ${ENG1104_COMMON}/lsm6ds3.c)
target_include_directories(lsm6ds3 PUBLIC ${ENG1104_COMMON})
target_link_libraries(lsm6ds3 PUBLIC pico_stdlib hardware_i2c)

add_library(ssd1306 STATIC ${ENG1104_COMMON}/ssd1306.c)
target_include_directories(ssd1306 PUBLIC ${ENG1104_COMMON})
target_link_libraries(ssd1306 PUBLIC pico_stdlib hardware_i2c)

add_library(actuators STATIC ${ENG1104_COMMON}/actuators.c)
target_include_directories(actuators PUBLIC ${ENG1104_COMMON})
target_link_libraries(actuators PUBLIC pico_stdlib hardware_pwm hardware_clocks)

# --- the project -----------------------------------------------------------
if(PROJECTILE_BUILD_APP)
    add_subdirectory(app)
endif()

# --- Week 8 ----------------------------------------------------------------
if(ENG1104_BUILD_W08)
    add_subdirectory(w08/i2c_scan)
    add_subdirectory(w08/gy511_read)
    add_subdirectory(w08/break_it)
    add_subdirectory(w08/heartbeat_trace)
    add_subdirectory(w08/oled)
endif()

# --- Week 9 ----------------------------------------------------------------
if(ENG1104_BUILD_W09)
    add_subdirectory(w09/imu)
    add_subdirectory(w09/gpio_irq)
    add_subdirectory(w09/timer_jitter)
    add_subdirectory(w09/pwm_buzzer)
    add_subdirectory(w09/servo)
    add_subdirectory(w09/sampler_50hz)
    add_subdirectory(w09/capture)
    add_subdirectory(w09/oled_bus)
endif()

# --- Week 10 ---------------------------------------------------------------
if(ENG1104_BUILD_W10)
    add_subdirectory(w10/imu_class)
endif()
