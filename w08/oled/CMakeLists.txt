add_executable(oled oled.c)

target_link_libraries(oled eng1104_board gy511 ssd1306 hardware_i2c)

pico_enable_stdio_usb(oled 1)
pico_enable_stdio_uart(oled 0)

pico_add_extra_outputs(oled)
