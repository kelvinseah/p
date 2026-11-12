/**
 * @file    oled_bus.c
 * @brief   ENG1104 Week 9 Ex07 -- two I2C controllers, and why you need both.
 *
 * By now the sampler runs from a hardware timer at a strict 50 Hz, and Ex02
 * showed how little jitter that leaves. This exercise adds a screen, which is
 * the first peripheral in the course big enough to threaten that.
 *
 * @section arithmetic Do the arithmetic before you wire anything
 *
 * A full 128x64 frame is 1024 bytes. Each byte costs 9 clock periods on the
 * wire (8 data bits plus the ACK), so at 400 kHz:
 *
 *     1024 x 9 / 400000 = 23 ms
 *
 * The sampling period is 20 ms. A whole-screen redraw is therefore @b longer
 * @b than @b the @b gap @b between @b samples. Share one bus between the
 * sensor and the screen and every redraw pushes a sample late.
 *
 * The RP2040 has two independent I2C controllers precisely so that this does
 * not have to happen:
 *
 *   - @b I2C0 on Grove 3 (GP4 / GP5)   -- the IMU, read from the timer ISR
 *   - @b I2C1 on Grove 6 (GP26 / GP27) -- the display, written from `main`
 *
 * @section measure What to look for
 *
 * The program prints the worst-case interval between samples. Let it settle,
 * then press button A to switch the screen between a full redraw and a
 * partial one, and watch what happens to that number -- and to the number of
 * pages actually sent.
 *
 * On two buses, neither redraw mode disturbs the sampler at all. That is the
 * result. Then try moving the display onto ::BOARD_I2C in `main` and see what
 * it costs -- the arithmetic above predicts the answer.
 *
 * @warning The ISR here does I2C to the @b sensor only, and only 12 bytes of
 *          it (~250 us). The screen is never touched from interrupt context.
 */
#include <stdio.h>

#include "board.h"
#include "lsm6ds3.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "ssd1306.h"

/** 50 Hz. Negative, so the period is measured start-to-start (see Week 9). */
#define SAMPLE_PERIOD_MS    (-20)

/** Redraw the screen five times a second. Human eyes need no more. */
#define SCREEN_PERIOD_MS    200

static volatile float    s_x, s_y, s_z;
static volatile uint32_t s_samples;
static volatile uint32_t s_worst_gap_us;
static volatile uint64_t s_last_us;

/**
 * @brief  Timer callback: one sensor read, and nothing else.
 *
 * Six bytes over I2C0 at 400 kHz is roughly 200 microseconds -- about 1% of
 * the 20 ms period.
 */
static bool on_sample(repeating_timer_t *rt)
{
    (void)rt;

    const uint64_t now = time_us_64();
    if (s_last_us != 0) {
        const uint32_t gap = (uint32_t)(now - s_last_us);
        if (gap > s_worst_gap_us) {
            s_worst_gap_us = gap;
        }
    }
    s_last_us = now;

    lsm6ds3_sample_t g;
    if (lsm6ds3_read(BOARD_I2C, &g)) {
        s_x = g.ax;
        s_y = g.ay;
        s_z = g.az;
    }
    s_samples++;
    return true;
}

int main(void)
{
    stdio_init_all();
    sleep_ms(2000);

    gpio_init(BOARD_BTN_A);
    gpio_set_dir(BOARD_BTN_A, GPIO_IN);
    gpio_pull_up(BOARD_BTN_A);

    printf("\nENG1104 Week 9 Ex07 -- sensor on I2C0, screen on I2C1\n");

    /* Bus 1 of 2: the sensor, on Grove 3. */
    i2c_init(BOARD_I2C, BOARD_I2C_BAUD);
    gpio_set_function(BOARD_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(BOARD_I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(BOARD_I2C_SDA);
    gpio_pull_up(BOARD_I2C_SCL);

    if (!lsm6ds3_init(BOARD_I2C)) {
        printf("IMU not found on Grove 3. Check the cable.\n");
        return 1;
    }

    /* Bus 2 of 2: the screen, on Grove 6. ssd1306_init() brings up its own
       pins, so nothing here touches the sensor's bus. */
    if (!ssd1306_init(BOARD_OLED_I2C, BOARD_OLED_SDA, BOARD_OLED_SCL,
                      BOARD_OLED_ADDR)) {
        printf("No display on Grove 6. Check VCC and GND before anything "
               "else -- they are adjacent pins.\n");
        return 1;
    }

    ssd1306_write(0, 0, "ENG1104 W9 EX07");
    ssd1306_write(1, 0, "SENSOR I2C0");
    ssd1306_write(2, 0, "SCREEN I2C1");
    ssd1306_flush();

    repeating_timer_t timer;
    add_repeating_timer_ms(SAMPLE_PERIOD_MS, on_sample, NULL, &timer);

    bool full_redraw = false;
    bool prev_btn = true;
    uint64_t next_screen = time_us_64();

    while (true) {
        /* Button A toggles full versus partial redraw. */
        const bool btn = gpio_get(BOARD_BTN_A);
        if (prev_btn && !btn) {
            full_redraw = !full_redraw;
        }
        prev_btn = btn;

        if (time_us_64() >= next_screen) {
            next_screen += SCREEN_PERIOD_MS * 1000u;

            char line[24];
            snprintf(line, sizeof line, "X %+5.2f Y %+5.2f", (double)s_x,
                     (double)s_y);
            ssd1306_write(4, 0, line);
            snprintf(line, sizeof line, "Z %+5.2f  N %5lu", (double)s_z,
                     (unsigned long)s_samples);
            ssd1306_write(5, 0, line);
            snprintf(line, sizeof line, "WORST GAP %5lu US",
                     (unsigned long)s_worst_gap_us);
            ssd1306_write(6, 0, line);
            /* Clear first: ssd1306_write() only overwrites the characters it
             * draws, so "FULL" over "PARTIAL" would leave "FULLIAL". */
            ssd1306_clear_row(7);
            ssd1306_write(7, 0, full_redraw ? "REDRAW: FULL"
                                            : "REDRAW: PARTIAL");

            const uint64_t t0 = time_us_64();
            const int pages = full_redraw ? ssd1306_flush_all()
                                          : ssd1306_flush();
            const uint32_t cost = (uint32_t)(time_us_64() - t0);

            printf("pages %d  redraw %6lu us   worst sample gap %6lu us\n",
                   pages, (unsigned long)cost,
                   (unsigned long)s_worst_gap_us);
        }
    }
}
