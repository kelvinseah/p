add_executable(heartbeat_trace heartbeat_trace.c)

target_link_libraries(heartbeat_trace eng1104_board gy511 hardware_i2c)

pico_enable_stdio_usb(heartbeat_trace 1)
pico_enable_stdio_uart(heartbeat_trace 0)

pico_add_extra_outputs(heartbeat_trace)
