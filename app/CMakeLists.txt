# projectile -- the project program. All of its code is in main.cpp.
add_executable(projectile main.cpp)

target_link_libraries(projectile
        pico_stdlib
        hardware_gpio
        actuators)        # servo_init(), servo_write_deg(), tone_play()

pico_enable_stdio_usb(projectile 1)
pico_enable_stdio_uart(projectile 0)

pico_add_extra_outputs(projectile)
