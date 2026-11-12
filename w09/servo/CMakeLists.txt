add_executable(servo servo.c)

target_link_libraries(servo eng1104_board actuators)

pico_enable_stdio_usb(servo 1)
pico_enable_stdio_uart(servo 0)

pico_add_extra_outputs(servo)
