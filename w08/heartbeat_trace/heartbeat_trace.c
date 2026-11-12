/**
 * @file    heartbeat_trace.c
 * @brief   ENG1104 Week 8 Ex04 -- see what your program is doing.
 *
 * Three of the lecture's debugging instruments in one program:
 *
 *   1. a heartbeat LED   -- one short blink per second proves the loop runs
 *   2. tagged printf()   -- [sample] and [impact] lines you can tell apart
 *   3. Teleplot -- the >mag: line draws |a| as a live graph
 *
 * Shake the board sharply: an [impact] line appears when |a| goes above
 * IMPACT_G. That is the first step of the fall detector in Lab Assessment 2.
 *
 * `loops` is declared volatile so a debugger can always show its value --
 * see the optional Debug Probe part of the self-study.
 */
#include <stdio.h>

#include "board.h"
#include "gy511.h"
#include "hardware/i2c.h"
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"

/** An impact is anything above this many g. */
#define IMPACT_G        2.0f

/** Loop period: 20 ms, so 50 loops make one second. */
#define PERIOD_MS       20u

/** LED used for the heartbeat. Not GP4/GP5 -- those carry the I2C bus. */
#define HEARTBEAT_LED   2u

/** Counts every pass through the loop. volatile: see the file comment. */
static volatile uint32_t loops = 0;

static void wait_for_serial_monitor(void)
{
    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }
    sleep_ms(200);
}

/**
 * @brief Drive the heartbeat LED: on for the first 100 ms of every second.
 *
 * Called once per loop. Uses the loop counter rather than a separate timer,
 * so if the loop ever stops, the LED stops blinking too -- which is the point.
 *
 * @param n  Current loop count.
 */
static void heartbeat_tick(uint32_t n)
{
    /* TODO(student): Light HEARTBEAT_LED for the first 100 ms of every second, off
         for the rest. The loop runs every PERIOD_MS, so n counts in
         steps of PERIOD_MS: use n % (1000 / PERIOD_MS) to find where
         you are inside the current second.
     */
}

int main(void)
{
    stdio_init_all();
    wait_for_serial_monitor();

    gpio_init(HEARTBEAT_LED);
    gpio_set_dir(HEARTBEAT_LED, GPIO_OUT);

    i2c_init(BOARD_I2C, BOARD_I2C_BAUD);
    gpio_set_function(BOARD_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(BOARD_I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(BOARD_I2C_SDA);
    gpio_pull_up(BOARD_I2C_SCL);

    printf("\nENG1104 W08 Ex04 -- heartbeat, tagged trace, plotter\n");
    if (!gy511_init(BOARD_I2C)) {
        printf("[error] gy511_init failed -- run i2c_scan\n");
    }

    while (true) {
        gy511_g_t g;
        if (gy511_read_g(BOARD_I2C, &g)) {
            float mag = gy511_magnitude(&g);

            printf(">mag:%.3f\n", (double)mag);          /* for the plotter */
            if ((loops % 50u) == 0u) {                    /* once a second */
                printf("[sample] x=%.2f y=%.2f z=%.2f |a|=%.2f\n",
                       (double)g.x, (double)g.y, (double)g.z, (double)mag);
            }
            if (mag > IMPACT_G) {
                printf("[impact] |a| = %.2f g\n", (double)mag);
            }
        } else {
            printf("[error] read failed\n");
        }

        heartbeat_tick(loops);
        loops++;
        sleep_ms(PERIOD_MS);
    }
}
