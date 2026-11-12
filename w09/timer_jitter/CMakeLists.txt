add_executable(timer_jitter timer_jitter.c)

target_link_libraries(timer_jitter eng1104_board lsm6ds3)

pico_enable_stdio_usb(timer_jitter 1)
pico_enable_stdio_uart(timer_jitter 0)

pico_add_extra_outputs(timer_jitter)
