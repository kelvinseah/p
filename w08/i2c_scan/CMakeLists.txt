add_executable(i2c_scan i2c_scan.c)

target_link_libraries(i2c_scan
        eng1104_board
        hardware_i2c)

pico_enable_stdio_usb(i2c_scan 1)
pico_enable_stdio_uart(i2c_scan 0)

pico_add_extra_outputs(i2c_scan)
