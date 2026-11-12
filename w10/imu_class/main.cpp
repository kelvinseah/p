/**
 * @file    main.cpp
 * @brief   ENG1104 Week 10 Ex01 -- the same sensor, now as an object.
 *
 * This program does exactly what the Week 9 Ex00 program did. Read them side
 * by side; the difference is entirely structural.
 *
 * @section c The C version, from Week 9
 *
 * @code
 *   if (!lsm6ds3_init(BOARD_I2C)) { ... }
 *   ...
 *   lsm6ds3_sample_t m;
 *   if (!lsm6ds3_read(BOARD_I2C, &m)) { ... }
 *   float a = lsm6ds3_accel_magnitude(&m);
 * @endcode
 *
 * Every call site has to know which I2C instance to pass, and has to remember
 * that init happened. Nothing stops you calling @c lsm6ds3_read() on an
 * uninitialised bus -- it will compile, run, and hand you rubbish.
 *
 * @section cpp The C++ version
 *
 * @code
 *   Imu imu(BOARD_I2C);
 *   imu.begin();
 *   ...
 *   Sample s;
 *   if (imu.read(s)) { float a = Imu::accelMagnitude(s); }
 * @endcode
 *
 * The I2C instance is stored once, at construction. The "am I initialised"
 * flag is private, so no caller can forget it or corrupt it. The unit
 * conversion belongs to the class rather than being copied into every file
 * that needs it.
 *
 * That is @b encapsulation: the data and the operations on it in one place,
 * with the details that callers should not touch made unreachable.
 */
#include <cstdio>

#include "Imu.h"
#include "board.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"

int main()
{
    stdio_init_all();
    sleep_ms(2500);

    i2c_init(BOARD_I2C, BOARD_I2C_BAUD);
    gpio_set_function(BOARD_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(BOARD_I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(BOARD_I2C_SDA);
    gpio_pull_up(BOARD_I2C_SCL);

    std::printf("\nENG1104 W10 Ex01 -- the IMU as a class\n");

    Imu imu(BOARD_I2C);

    if (!imu.begin()) {
        std::printf("IMU not found -- is it in Grove 3? Run the W08 I2C scan\n");
        while (true) {
            tight_loop_contents();
        }
    }

    /* Try this: comment out imu.begin() above and run again. read() returns
     * false rather than handing you meaningless numbers, because the object
     * knows its own state. The C version could not do that. */

    while (true) {
        Sample s;

        if (imu.read(s)) {
            std::printf("a %+6.2f %+6.2f %+6.2f g  |a| %5.2f    "
                        "|w| %6.1f dps\n",
                        static_cast<double>(s.ax),
                        static_cast<double>(s.ay),
                        static_cast<double>(s.az),
                        static_cast<double>(Imu::accelMagnitude(s)),
                        static_cast<double>(Imu::gyroMagnitude(s)));
        } else {
            std::printf("read failed\n");
        }

        sleep_ms(200);
    }

    return 0;
}
