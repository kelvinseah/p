add_executable(oled_bus oled_bus.c)

target_link_libraries(oled_bus eng1104_board lsm6ds3 ssd1306)

pico_enable_stdio_usb(oled_bus 1)
pico_enable_stdio_uart(oled_bus 0)

pico_add_extra_outputs(oled_bus)
