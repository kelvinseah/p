add_executable(imu imu.c)

target_link_libraries(imu eng1104_board lsm6ds3)

pico_enable_stdio_usb(imu 1)
pico_enable_stdio_uart(imu 0)

pico_add_extra_outputs(imu)
