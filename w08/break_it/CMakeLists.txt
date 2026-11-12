add_executable(break_it break_it.c)

target_link_libraries(break_it eng1104_board ssd1306 hardware_i2c)

pico_enable_stdio_usb(break_it 1)
pico_enable_stdio_uart(break_it 0)

pico_add_extra_outputs(break_it)
