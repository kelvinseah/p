/**
 * @file    timer_jitter.c
 * @brief   ENG1104 Week 9 Ex02 -- sleep_ms() is not a sampling clock.
 *
 * This is the measurement that justifies everything else in Week 9. Three
 * ways of "sampling at 50 Hz" run in turn, each for 200 samples, and the
 * actual interval between samples is measured with @c time_us_64().
 *
 *   A  sleep_ms(20) with no work        -> close to 20 ms, but only because
 *                                          nothing happens in the loop
 *   B  sleep_ms(20) with a real read    -> 20 ms PLUS however long the work
 *                                          took, every single time
 *   C  add_repeating_timer_ms(-20, ...) -> locked to 20 ms regardless of the
 *                                          work
 *
 * @section jitter_sign The sign of the delay
 *
 * @c add_repeating_timer_ms() interprets its first argument differently
 * depending on its sign, and this is the whole trick:
 *
 *   - @b positive: wait this long @e after the callback returns. The period
 *     becomes 20 ms + however long the callback took, so it drifts.
 *   - @b negative: fire again exactly this long after the callback @e started.
 *     The period is fixed and the work happens inside it.
 *
 * A machine-learning model trained on evenly spaced data expects evenly
 * spaced data at inference time, so C is the only acceptable option.
 */
#include <stdio.h>

#include "board.h"
#include "lsm6ds3.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "pico/time.h"

/** Target sampling period. 20 ms = 50 Hz. */
#define PERIOD_MS   20
/** Samples measured per experiment. */
#define N_SAMPLES   200

/** Interval statistics for one experiment, in microseconds. */
typedef struct {
    uint32_t min;
    uint32_t max;
    uint64_t sum;
    uint32_t count;
} stats_t;

static void stats_reset(stats_t *s)
{
    s->min = UINT32_MAX;
    s->max = 0;
    s->sum = 0;
    s->count = 0;
}

/**
 * @brief Fold one measured interval into a ::stats_t.
 *
 * @param s   Statistics to update.
 * @param us  Measured interval in microseconds.
 */
static void stats_add(stats_t *s, uint32_t us)
{
    if (us < s->min) {
        s->min = us;
    }
    if (us > s->max) {
        s->max = us;
    }
    s->sum += us;
    s->count++;
}

/**
 * @brief Print one experiment's result.
 *
 * @param label  Name of the experiment.
 * @param s      Collected statistics.
 */
static void stats_report(const char *label, const stats_t *s)
{
    if (s->count == 0) {
        printf("%-34s no samples\n", label);
        return;
    }

    uint32_t mean = (uint32_t)(s->sum / s->count);

    printf("%-34s mean %6u us (%5.2f Hz)   min %6u   max %6u   "
           "spread %5u us\n",
           label, (unsigned)mean, 1e6 / (double)mean,
           (unsigned)s->min, (unsigned)s->max,
           (unsigned)(s->max - s->min));
}

/* ------------------------------------------------------------------ */
/* Experiment C state -- written by the timer callback, read by main() */
/* ------------------------------------------------------------------ */
static volatile stats_t g_timer_stats;
static volatile uint64_t g_timer_last_us = 0;
static volatile bool g_timer_done = false;

/**
 * @brief Repeating-timer callback: sample the sensor and time the interval.
 *
 * Runs in interrupt context. It does the I2C read and the arithmetic, and
 * nothing else -- no printing.
 *
 * @param t  Timer that fired (unused).
 * @return true to keep the timer running, false to cancel it.
 */
static bool timer_cb(struct repeating_timer *t)
{
    (void)t;

    uint64_t now = time_us_64();

    lsm6ds3_raw_t raw;
    (void)lsm6ds3_read_raw(BOARD_I2C, &raw);   /* the "work" */

    if (g_timer_last_us != 0) {
        stats_add((stats_t *)&g_timer_stats, (uint32_t)(now - g_timer_last_us));
    }
    g_timer_last_us = now;

    if (g_timer_stats.count >= N_SAMPLES) {
        g_timer_done = true;
        return false;                        /* cancel the timer */
    }
    return true;
}

int main(void)
{
    stdio_init_all();
    sleep_ms(2500);

    i2c_init(BOARD_I2C, BOARD_I2C_BAUD);
    gpio_set_function(BOARD_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(BOARD_I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(BOARD_I2C_SDA);
    gpio_pull_up(BOARD_I2C_SCL);

    printf("\nENG1104 W09 Ex02 -- is your 50 Hz really 50 Hz?\n");
    printf("target: %d ms = %.1f Hz, %d samples per experiment\n\n",
           PERIOD_MS, 1000.0 / PERIOD_MS, N_SAMPLES);

    bool have_sensor = lsm6ds3_init(BOARD_I2C);
    if (!have_sensor) {
        printf("NOTE: no IMU in Grove 3; experiment B will understate\n");
        printf("      the problem because the I2C read fails immediately.\n\n");
    }

    /* ---- A: sleep_ms() with an empty loop --------------------------- */
    {
        stats_t s;
        stats_reset(&s);
        uint64_t last = time_us_64();

        for (int i = 0; i < N_SAMPLES; i++) {
            sleep_ms(PERIOD_MS);
            uint64_t now = time_us_64();
            stats_add(&s, (uint32_t)(now - last));
            last = now;
        }
        stats_report("A  sleep_ms, no work", &s);
    }

    /* ---- B: sleep_ms() with a real sensor read ---------------------- */
    {
        stats_t s;
        stats_reset(&s);
        uint64_t last = time_us_64();

        for (int i = 0; i < N_SAMPLES; i++) {
            lsm6ds3_raw_t raw;
            (void)lsm6ds3_read_raw(BOARD_I2C, &raw);

            sleep_ms(PERIOD_MS);

            uint64_t now = time_us_64();
            stats_add(&s, (uint32_t)(now - last));
            last = now;
        }
        stats_report("B  sleep_ms + I2C read", &s);
    }

    /* ---- C: hardware repeating timer, negative delay ----------------- */
    {
        stats_reset((stats_t *)&g_timer_stats);
        g_timer_last_us = 0;
        g_timer_done = false;

        struct repeating_timer timer;
        add_repeating_timer_ms(-PERIOD_MS, timer_cb, NULL, &timer);

        while (!g_timer_done) {
            tight_loop_contents();
        }
        stats_report("C  repeating timer (-20 ms)", (stats_t *)&g_timer_stats);
    }

    printf("\nA and B drift because the period is 20 ms PLUS the work.\n");
    printf("C is locked because the deadline is measured from the START\n");
    printf("of the previous callback -- that is what the negative sign means.\n");

    while (true) {
        tight_loop_contents();
    }

    return 0;
}
