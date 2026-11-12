add_executable(gy511_read gy511_read.c)

target_link_libraries(gy511_read eng1104_board gy511)

pico_enable_stdio_usb(gy511_read 1)
pico_enable_stdio_uart(gy511_read 0)

pico_add_extra_outputs(gy511_read)
