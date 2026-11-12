add_executable(capture capture.c)

target_link_libraries(capture eng1104_board lsm6ds3 actuators
                      pico_flash hardware_flash)

pico_enable_stdio_usb(capture 1)
pico_enable_stdio_uart(capture 0)

pico_add_extra_outputs(capture)
