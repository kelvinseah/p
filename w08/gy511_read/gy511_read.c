/**
 * @file    gy511_read.c
 * @brief   ENG1104 Week 8 Ex02 -- read the accelerometer and print it.
 *
 * Prints each sample three ways so the relationship between them is obvious:
 *
 *   raw     the 16-bit words straight off the sensor
 *   g       the same numbers scaled to multiples of gravity
 *   plot    Teleplot format, which the Teleplot extension draws as a graph
 *
 * Lay the board flat and one axis should read about 1 g while the other two
 * read about 0. Turn it on each edge in turn and confirm the 1 g moves to the
 * axis you expect. If it does not, the module is mounted differently from the
 * one in the lecture slides -- note which axis is which, because Lab
 * Assessment 2 depends on knowing it.
 */
#include <stdio.h>

#include "board.h"
#include "gy511.h"
#include "hardware/i2c.h"
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"

/** Sampling period. 20 ms is 50 Hz, matching the sensor's configured rate. */
#define PERIOD_MS 20u

/** Print every Nth sample so the text stays readable at 50 Hz. */
#define PRINT_EVERY 10u

int main(void)
{
    stdio_init_all();
    /* Wait for the Serial Monitor so the first lines are not lost. */
    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }
    sleep_ms(200);

    i2c_init(BOARD_I2C, BOARD_I2C_BAUD);
    gpio_set_function(BOARD_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(BOARD_I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(BOARD_I2C_SDA);
    gpio_pull_up(BOARD_I2C_SCL);

    printf("\nENG1104 W08 Ex02 -- GY-511 accelerometer\n");

    if (!gy511_init(BOARD_I2C)) {
        printf("gy511_init() FAILED.\n");
        printf("Run i2c_scan first -- if 0x19 does not appear there,\n");
        printf("this is a wiring problem, not a code problem.\n");
        while (true) {
            tight_loop_contents();
        }
    }

    printf("configured: 50 Hz, +/-8 g, high resolution, BDU on\n\n");
    sleep_ms(30);   /* at 50 Hz the first sample is ready after 20 ms */

    uint32_t n = 0;

    while (true) {
        gy511_raw_t raw;
        gy511_g_t g;

        if (!gy511_read_raw(BOARD_I2C, &raw)) {
            printf("read failed\n");
            sleep_ms(500);
            continue;
        }

        g.x = gy511_to_g(raw.x);
        g.y = gy511_to_g(raw.y);
        g.z = gy511_to_g(raw.z);

        if ((n % PRINT_EVERY) == 0u) {
            printf("raw % 7d % 7d % 7d   g % 6.2f % 6.2f % 6.2f   |a| %5.2f\n",
                   raw.x, raw.y, raw.z,
                   (double)g.x, (double)g.y, (double)g.z,
                   (double)gy511_magnitude(&g));
        }

        /* Teleplot format -- the Teleplot extension turns these into
         * live traces. One variable per line, ">name:value". */
        printf(">x:%.3f\n>y:%.3f\n>z:%.3f\n",
               (double)g.x, (double)g.y, (double)g.z);

        n++;
        sleep_ms(PERIOD_MS);   /* NOT actually 50 Hz -- see Week 9. */
    }

    return 0;
}
