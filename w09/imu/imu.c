/**
 * @file    imu.c
 * @brief   ENG1104 Week 9 Ex00 -- the IMU, and what the gyroscope adds.
 *
 * The IMU measures two different things:
 *
 *   accelerometer  ax ay az   in g     -- straight-line acceleration, plus
 *                                         gravity
 *   gyroscope      gx gy gz   in dps   -- how fast the board is turning,
 *                                         in degrees per second
 *
 * The GY-511 you used in Week 8 had only the first. This program prints both,
 * plus one number for each -- the size of the acceleration |a| and the size
 * of the turning rate |w| -- so you can see for yourself what each one reacts
 * to.
 *
 * @section try Try this, with the IMU fixed to the board
 *
 *   1. Leave the board flat on the bench.
 *   2. Push it along the bench without turning it.
 *   3. Leave it flat and spin it on the spot, like turning a jar lid.
 *
 * What we measured on 9 Oct 2026:
 *
 *   still     |a| about 1.0 g        |w| a few dps
 *   pushed    |a| 0.9 - 1.5 g        |w| tens of dps, up to about 100
 *   spun      |a| up to several g    |w| hundreds to 2000 dps (the limit
 *                                        of the range this driver sets)
 *
 * Pushing barely moves |w|; turning moves it twenty times as much. A fall
 * turns the body through about 90 degrees in a fraction of a second; sitting
 * down heavily gives a similar jolt to the accelerometer but almost no
 * turning. That difference is why the IMU is your sensor from this week on.
 *
 * Open Teleplot to see the six channels and the two magnitudes as graphs.
 */
#include <stdio.h>

#include "board.h"
#include "hardware/i2c.h"
#include "lsm6ds3.h"
#include "pico/stdlib.h"

/** Sampling period. 20 ms is 50 Hz -- but see Ex05 for why sleep_ms is not. */
#define PERIOD_MS 20u

/** Print the human-readable line every Nth sample so the text stays legible. */
#define PRINT_EVERY 10u

int main(void)
{
    stdio_init_all();
    sleep_ms(2500);

    i2c_init(BOARD_I2C, BOARD_I2C_BAUD);
    gpio_set_function(BOARD_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(BOARD_I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(BOARD_I2C_SDA);
    gpio_pull_up(BOARD_I2C_SCL);

    printf("\nENG1104 W09 Ex00 -- the IMU, and what the gyroscope adds\n");

    if (!lsm6ds3_init(BOARD_I2C)) {
        printf("IMU at 0x%02X: NOT FOUND\n\n", BOARD_IMU_ADDR);
        printf("Run i2c_scan from Week 8. 0x6A must appear. If it\n");
        printf("does not, check the Grove cable is in Grove 3 at both ends.\n");
        while (true) {
            tight_loop_contents();
        }
    }
    printf("IMU at 0x%02X: ready\n\n", BOARD_IMU_ADDR);

    uint32_t n = 0;

    while (true) {
        lsm6ds3_sample_t s;
        if (lsm6ds3_read(BOARD_I2C, &s)) {
            const float a = lsm6ds3_accel_magnitude(&s);
            const float w = lsm6ds3_gyro_magnitude(&s);

            if ((n % PRINT_EVERY) == 0u) {
                printf("a % 6.2f % 6.2f % 6.2f g  |a| %5.2f     "
                       "w % 7.1f % 7.1f % 7.1f dps  |w| %6.1f\n",
                       (double)s.ax, (double)s.ay, (double)s.az, (double)a,
                       (double)s.gx, (double)s.gy, (double)s.gz, (double)w);
            }

            /* Teleplot: one variable per line, ">name:value". */
            printf(">ax:%.3f\n>ay:%.3f\n>az:%.3f\n",
                   (double)s.ax, (double)s.ay, (double)s.az);
            printf(">gx:%.1f\n>gy:%.1f\n>gz:%.1f\n",
                   (double)s.gx, (double)s.gy, (double)s.gz);
            printf(">a_mag:%.3f\n>w_mag:%.1f\n", (double)a, (double)w);
        }

        n++;
        sleep_ms(PERIOD_MS);
    }

    return 0;
}
