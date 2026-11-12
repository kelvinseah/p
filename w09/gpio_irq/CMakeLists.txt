add_executable(gpio_irq gpio_irq.c)

target_link_libraries(gpio_irq eng1104_board)

pico_enable_stdio_usb(gpio_irq 1)
pico_enable_stdio_uart(gpio_irq 0)

pico_add_extra_outputs(gpio_irq)
