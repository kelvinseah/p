add_executable(imu_class
        main.cpp
        Imu.cpp)

target_link_libraries(imu_class
        eng1104_board
        lsm6ds3)

pico_enable_stdio_usb(imu_class 1)
pico_enable_stdio_uart(imu_class 0)

pico_add_extra_outputs(imu_class)
