add_executable(pwm_buzzer pwm_buzzer.c)

target_link_libraries(pwm_buzzer eng1104_board actuators)

pico_enable_stdio_usb(pwm_buzzer 1)
pico_enable_stdio_uart(pwm_buzzer 0)

pico_add_extra_outputs(pwm_buzzer)
