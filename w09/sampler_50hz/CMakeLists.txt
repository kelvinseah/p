add_executable(sampler_50hz sampler_50hz.c)

target_link_libraries(sampler_50hz eng1104_board lsm6ds3)

pico_enable_stdio_usb(sampler_50hz 1)
pico_enable_stdio_uart(sampler_50hz 0)

pico_add_extra_outputs(sampler_50hz)
