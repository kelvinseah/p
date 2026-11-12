/**
 * @file    sampler_50hz.c
 * @brief   ENG1104 Week 9 Ex05 -- a deterministic 50 Hz sampler.
 *
 * This is the piece of firmware Lab Assessment 2 is built on. It separates
 * two jobs that must not interfere with each other:
 *
 *   - a @b timer @b interrupt reads the IMU -- three accelerometer and three
 *     gyroscope axes -- every 20 ms and drops the sample into a ring buffer. It does nothing else, so it always
 *     finishes long before the next deadline.
 *   - the @b main @b loop drains the buffer and prints. Printing is slow and
 *     unpredictable, which is exactly why it must not happen in the ISR.
 *
 * @section ring Why a ring buffer
 *
 * The ISR produces and @c main() consumes, at different and unrelated rates.
 * A ring buffer decouples them: a burst of slow printing does not stall the
 * sampler, it only makes the buffer fill up. If it ever fills completely a
 * sample is dropped, and ::g_dropped counts that -- a non-zero value there
 * means the consumer is too slow and the recording is no longer evenly
 * spaced.
 *
 * Only one index is written by each side (@c head by the ISR, @c tail by
 * @c main), so on a single core no locking is needed. Both must be
 * @c volatile or the compiler may cache them.
 */
#include <stdio.h>

#include "board.h"
#include "hardware/i2c.h"
#include "lsm6ds3.h"
#include "pico/stdlib.h"
#include "pico/time.h"

/** Sampling period in milliseconds. 20 ms = 50 Hz. */
#define PERIOD_MS   20

/** Ring buffer capacity. Power of two so the wrap is a mask, not a divide. */
#define RING_SIZE   64u
#define RING_MASK   (RING_SIZE - 1u)

/** One time-stamped sample. */
typedef struct {
    uint64_t    t_us;   /**< Capture time, microseconds since boot. */
    lsm6ds3_raw_t m;    /**< Raw counts, all six axes. */
} sample_t;

static sample_t g_ring[RING_SIZE];
static volatile uint32_t g_head = 0;    /**< Written by the ISR only. */
static volatile uint32_t g_tail = 0;    /**< Written by main() only. */
static volatile uint32_t g_dropped = 0; /**< Samples lost to a full buffer. */
static volatile uint32_t g_read_fail = 0;

/* Interval statistics, all updated inside the ISR. */
static volatile uint64_t g_prev_us = 0;
static volatile uint32_t g_min_us = UINT32_MAX;
static volatile uint32_t g_max_us = 0;

/**
 * @brief Timer callback -- read one sample and queue it.
 *
 * Runs in interrupt context every ::PERIOD_MS. Total cost is one twelve-byte
 * I2C burst read (about 250 us at 400 kHz) plus a handful of stores, so it
 * uses a little over 1 % of the 20 ms budget.
 *
 * @param t  Timer that fired (unused).
 * @return true, to keep the timer running.
 */
static bool sample_cb(struct repeating_timer *t)
{
    /* TODO(student): Runs in interrupt context every PERIOD_MS.
         1. record the interval since g_prev_us into g_min_us/g_max_us
         2. read the IMU with lsm6ds3_read_raw()
         3. push the time-stamped sample into the ring buffer
         If the buffer is full, increment g_dropped and return --
         never block here. Return true to keep the timer running.
     */
    return false;
}

/**
 * @brief Take one sample from the ring buffer, if there is one.
 *
 * @param out  Destination.
 * @return true if a sample was returned.
 */
static bool ring_pop(sample_t *out)
{
    /* TODO(student): Return false if g_tail == g_head (buffer empty). Otherwise copy
         g_ring[g_tail] into *out, advance g_tail with & RING_MASK,
         and return true.
     */
    return false;
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

    printf("\nENG1104 W09 Ex05 -- deterministic 50 Hz sampler\n");

    if (!lsm6ds3_init(BOARD_I2C)) {
        printf("lsm6ds3_init() FAILED -- is the IMU in Grove 3? The W08 I2C\n"
               "scan must show 0x6A.\n");
        while (true) {
            tight_loop_contents();
        }
    }

    /* Negative delay: fire again exactly PERIOD_MS after the START of the
     * previous callback, so the work inside it does not push the deadline. */
    struct repeating_timer timer;
    if (!add_repeating_timer_ms(-PERIOD_MS, sample_cb, NULL, &timer)) {
        printf("could not start the sampling timer\n");
        return -1;
    }

    printf("sampling at %d ms; ISR queues, main() prints\n\n", PERIOD_MS);

    uint32_t n = 0;

    while (true) {
        sample_t s;

        if (!ring_pop(&s)) {
            tight_loop_contents();      /* buffer empty -- wait for the ISR */
            continue;
        }

        /* Teleplot lines: three accelerometer axes in g, three gyroscope
         * axes in degrees per second. */
        printf(">ax:%.3f\n>ay:%.3f\n>az:%.3f\n",
               (double)lsm6ds3_to_g(s.m.ax),
               (double)lsm6ds3_to_g(s.m.ay),
               (double)lsm6ds3_to_g(s.m.az));
        printf(">gx:%.1f\n>gy:%.1f\n>gz:%.1f\n",
               (double)lsm6ds3_to_dps(s.m.gx),
               (double)lsm6ds3_to_dps(s.m.gy),
               (double)lsm6ds3_to_dps(s.m.gz));

        /* Once a second, report how well the timing held up. */
        if ((++n % 50u) == 0u) {
            printf("# %5u samples   interval %u..%u us   dropped %u   "
                   "i2c errors %u\n",
                   (unsigned)n, (unsigned)g_min_us, (unsigned)g_max_us,
                   (unsigned)g_dropped, (unsigned)g_read_fail);
        }
    }

    return 0;
}
