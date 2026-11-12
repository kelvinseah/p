/**
 * @file    capture.c
 * @brief   ENG1104 Week 9 Ex06 -- record labelled telemetry for Edge Impulse.
 *
 * Same deterministic 50 Hz sampler as Ex05, wrapped in a recorder. One button
 * press captures one fixed-length window; the other button chooses which
 * label the window gets.
 *
 * Every window is both **printed over USB** and **saved in the Pico's
 * flash**. With a terminal open you see each window as it is recorded. On
 * battery, with no computer attached, nothing is lost: the windows wait in
 * flash, survive power-off, and are printed again when you ask for them.
 *
 * @section cap_use How to use it
 *
 *   GP21  cycle the label:  idle -> walking -> fall -> idle ...
 *   GP20  record one window: one beep, 1 s to get ready, ::WINDOW_MS of
 *         recording (all LEDs lit), two beeps when it is saved
 *
 * The LEDs show the current label (1 lit = idle, 2 = walking, 3 = fall).
 *
 * In the serial terminal:
 *
 *   d   print every window saved in flash
 *   e   erase them all (asks you to confirm with y)
 *   h   show these commands and how many windows are saved
 *
 * Each window is printed like this, ready to paste into a @c .csv file:
 *
 * @code
 *   # BEGIN label=walking samples=100 rate=50Hz
 *   timestamp,accX,accY,accZ,gyrX,gyrY,gyrZ
 *   0,0.0120,-0.0340,0.9980,2.7,-4.6,-1.5
 *   20,0.0190,-0.0410,1.0020,3.1,-4.2,-1.8
 *   ...
 *   # END label=walking dropped=0
 * @endcode
 *
 * The timestamp column is milliseconds from the start of the window, which is
 * the format the Edge Impulse CSV wizard expects. Acceleration is in g, angular
 * rate in degrees per second -- all six channels, so the decision about which
 * ones the model uses is made in Edge Impulse, not here.
 *
 * @section cap_flash Where the windows are kept
 *
 * The last ::STORE_BYTES of the 2 MB flash, one 4 KB sector per window --
 * room for ::STORE_SLOTS windows. The program itself is far smaller and sits
 * at the start of flash, so flashing a new .uf2 does not touch the saved
 * windows. Writing to flash pauses interrupts for a few tens of
 * milliseconds, so it is only ever done between recordings.
 *
 * @section cap_advice Collecting data that is worth training on
 *
 *   - Fix the IMU to the board or pack @b before the first recording, and
 *     keep it in that position for every recording and for the
 *     demonstration. A module that wobbles on its cable records its own
 *     wobble, and a model trained with the arrow pointing one way will not
 *     recognise data recorded with it pointing another.
 *   - Aim for at least 30 windows of each class, from more than one person.
 *   - "idle" must include sitting, standing and lying still.
 *   - "walking" must include normal walking, fast walking and stairs.
 *   - "fall" is the board, held the way it is worn, tipped over by hand
 *     onto a mat or a cushion. @b Nobody falls to record it, and never
 *     onto a hard floor.
 *   - Record the awkward cases too -- sitting down heavily, dropping the
 *     board on a cushion. Those are what a threshold gets wrong, and they are
 *     what the model has to learn to reject.
 */
#include <stdio.h>
#include <string.h>

#include "actuators.h"
#include "board.h"
#include "hardware/flash.h"
#include "hardware/i2c.h"
#include "lsm6ds3.h"
#include "pico/flash.h"
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"
#include "pico/time.h"

/** Sampling period in milliseconds. */
#define PERIOD_MS       20
/** Window length in milliseconds. 2000 ms at 50 Hz = 100 samples. */
#define WINDOW_MS       2000
/** Samples per window. */
#define WINDOW_SAMPLES  (WINDOW_MS / PERIOD_MS)
/** Time between the start beep and the start of the recording. */
#define GET_READY_MS    1000
/** Button debounce, in microseconds. */
#define DEBOUNCE_US     250000u

/** Flash set aside for saved windows: the last 1 MB. */
#define STORE_BYTES     (1024u * 1024u)
/** Where that area starts, as an offset from the start of flash. */
#define STORE_OFFSET    (PICO_FLASH_SIZE_BYTES - STORE_BYTES)
/** One window per erasable sector. */
#define STORE_SLOTS     (STORE_BYTES / FLASH_SECTOR_SIZE)
/** Marks a sector that holds a saved window. */
#define STORE_MAGIC     0x45313036u   /* "E106" */

/** Class labels. Keep these identical to the ones used in Edge Impulse. */
static const char *const LABELS[] = { "idle", "walking", "fall" };
#define LABEL_COUNT (sizeof(LABELS) / sizeof(LABELS[0]))

static const uint LED_PINS[] = BOARD_LED_PINS_NO_I2C;
#define LED_COUNT (sizeof(LED_PINS) / sizeof(LED_PINS[0]))

/** One saved window, exactly as it is laid out in flash. */
typedef struct {
    uint32_t magic;                           /**< ::STORE_MAGIC. */
    uint32_t label;                           /**< Index into ::LABELS. */
    uint32_t dropped;                         /**< Failed reads. */
    lsm6ds3_sample_t s[WINDOW_SAMPLES];       /**< The samples. */
} saved_window_t;

/* Flash is programmed in whole 256-byte pages. */
#define SAVED_BYTES \
    ((sizeof(saved_window_t) + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE \
     * FLASH_PAGE_SIZE)
_Static_assert(SAVED_BYTES <= FLASH_SECTOR_SIZE,
               "one window must fit in one flash sector");

/** The window being recorded, padded to whole flash pages. */
static union {
    saved_window_t w;
    uint8_t bytes[SAVED_BYTES];
} g_rec;

static volatile uint32_t g_index = 0;       /**< Samples captured so far. */
static volatile bool g_recording = false;
static volatile bool g_window_ready = false;
static volatile uint32_t g_dropped = 0;
static uint32_t g_label = 0;
static uint32_t g_saved = 0;                /**< Windows in flash. */

/* Button edge flags, set by the GPIO ISR, cleared by main(). */
static volatile bool g_btn_record = false;
static volatile bool g_btn_label = false;

/**
 * @brief GPIO interrupt handler for both user buttons.
 *
 * Sets a flag and returns -- all the real work happens in @c main().
 *
 * @param gpio    GPIO that fired.
 * @param events  Event mask (unused).
 */
static void button_isr(uint gpio, uint32_t events)
{
    (void)events;

    static uint64_t last_a = 0;
    static uint64_t last_b = 0;

    const uint64_t now = time_us_64();

    if (gpio == BOARD_BTN_A && now - last_a > DEBOUNCE_US) {
        last_a = now;
        g_btn_record = true;
    } else if (gpio == BOARD_BTN_B && now - last_b > DEBOUNCE_US) {
        last_b = now;
        g_btn_label = true;
    }
}

/**
 * @brief Timer callback -- capture one sample if a recording is running.
 *
 * @param t  Timer that fired (unused).
 * @return true, to keep the timer running.
 */
static bool sample_cb(struct repeating_timer *t)
{
    (void)t;

    if (!g_recording) {
        return true;
    }

    lsm6ds3_sample_t g;
    if (!lsm6ds3_read(BOARD_I2C, &g)) {
        g_dropped++;
        return true;
    }

    if (g_index < WINDOW_SAMPLES) {
        g_rec.w.s[g_index++] = g;
    }

    if (g_index >= WINDOW_SAMPLES) {
        g_recording = false;
        g_window_ready = true;
    }
    return true;
}

/**
 * @brief Light the first @p n LEDs and clear the rest.
 */
static void leds_show(uint n)
{
    for (uint i = 0; i < LED_COUNT; i++) {
        gpio_put(LED_PINS[i], i < n);
    }
}

/**
 * @brief Sound the buzzer @p count times -- feedback you can hear with the
 *        board on your waist.
 */
static void beep(uint count)
{
    for (uint i = 0; i < count; i++) {
        tone_play(BOARD_BUZZER, 2000.0f, 0.5f);
        sleep_ms(80);
        tone_stop(BOARD_BUZZER);
        sleep_ms(80);
    }
}

/** @return the saved window in slot @p slot, read straight from flash. */
static const saved_window_t *slot_ptr(uint32_t slot)
{
    return (const saved_window_t *)(XIP_BASE + STORE_OFFSET +
                                    slot * FLASH_SECTOR_SIZE);
}

/** @return how many windows are saved: the slots in use are always 0..n-1. */
static uint32_t count_saved(void)
{
    uint32_t n = 0;
    while (n < STORE_SLOTS && slot_ptr(n)->magic == STORE_MAGIC) {
        n++;
    }
    return n;
}

/* Runs with interrupts paused (see flash_safe_execute). */
static void write_slot(void *param)
{
    const uint32_t off = STORE_OFFSET + (uint32_t)(uintptr_t)param;
    flash_range_erase(off, FLASH_SECTOR_SIZE);
    flash_range_program(off, g_rec.bytes, SAVED_BYTES);
}

static void erase_slot(void *param)
{
    flash_range_erase(STORE_OFFSET + (uint32_t)(uintptr_t)param,
                      FLASH_SECTOR_SIZE);
}

/**
 * @brief Save the window just recorded into the next free slot.
 * @return true if it was saved.
 */
static bool save_window(void)
{
    if (g_saved >= STORE_SLOTS) {
        return false;
    }
    const uintptr_t off = (uintptr_t)g_saved * FLASH_SECTOR_SIZE;
    if (flash_safe_execute(write_slot, (void *)off, 1000) != PICO_OK) {
        return false;
    }
    g_saved++;
    return true;
}

/** @brief Erase every saved window. */
static void erase_all(void)
{
    for (uint32_t i = 0; i < g_saved; i++) {
        flash_safe_execute(erase_slot,
                           (void *)((uintptr_t)i * FLASH_SECTOR_SIZE), 1000);
    }
    g_saved = 0;
}

/**
 * @brief Print one window as CSV.
 */
static void print_window(const saved_window_t *w)
{
    printf("\n# BEGIN label=%s samples=%u rate=%dHz\n",
           LABELS[w->label], (unsigned)WINDOW_SAMPLES, 1000 / PERIOD_MS);
    printf("timestamp,accX,accY,accZ,gyrX,gyrY,gyrZ\n");

    for (uint32_t i = 0; i < WINDOW_SAMPLES; i++) {
        printf("%u,%.4f,%.4f,%.4f,%.1f,%.1f,%.1f\n",
               (unsigned)(i * PERIOD_MS),
               (double)w->s[i].ax,
               (double)w->s[i].ay,
               (double)w->s[i].az,
               (double)w->s[i].gx,
               (double)w->s[i].gy,
               (double)w->s[i].gz);
    }

    printf("# END label=%s dropped=%u\n\n",
           LABELS[w->label], (unsigned)w->dropped);
}

static void print_help(void)
{
    printf("\nENG1104 W09 Ex06 -- telemetry capture\n");
    printf("GP%u = record one %d ms window,  GP%u = change label\n",
           (unsigned)BOARD_BTN_A, WINDOW_MS, (unsigned)BOARD_BTN_B);
    printf("type  d = print saved windows,  e = erase them,  h = help\n");
    printf("saved in flash: %u of %u windows\n", (unsigned)g_saved,
           (unsigned)STORE_SLOTS);
    printf("label is now: %s\n\n", LABELS[g_label]);
}

/** @brief Handle one character typed in the serial terminal. */
static void handle_command(int c)
{
    if (c == 'd') {
        printf("\n# %u saved windows\n", (unsigned)g_saved);
        for (uint32_t i = 0; i < g_saved; i++) {
            print_window(slot_ptr(i));
        }
    } else if (c == 'e') {
        printf("erase all %u saved windows? type y to confirm\n",
               (unsigned)g_saved);
        if (getchar_timeout_us(10 * 1000 * 1000) == 'y') {
            erase_all();
            printf("erased\n");
        } else {
            printf("not erased\n");
        }
    } else if (c == 'h') {
        print_help();
    }
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

    for (uint i = 0; i < LED_COUNT; i++) {
        gpio_init(LED_PINS[i]);
        gpio_set_dir(LED_PINS[i], GPIO_OUT);
        gpio_put(LED_PINS[i], 0);
    }

    gpio_init(BOARD_BTN_A);
    gpio_set_dir(BOARD_BTN_A, GPIO_IN);
    gpio_pull_up(BOARD_BTN_A);
    gpio_init(BOARD_BTN_B);
    gpio_set_dir(BOARD_BTN_B, GPIO_IN);
    gpio_pull_up(BOARD_BTN_B);

    gpio_set_irq_enabled_with_callback(BOARD_BTN_A, GPIO_IRQ_EDGE_FALL, true,
                                       &button_isr);
    gpio_set_irq_enabled(BOARD_BTN_B, GPIO_IRQ_EDGE_FALL, true);

    g_saved = count_saved();

    if (!lsm6ds3_init(BOARD_I2C)) {
        printf("lsm6ds3_init() FAILED -- is the IMU in Grove 3? The W08 I2C\n"
               "scan must show 0x6A.\n");
        while (true) {
            tight_loop_contents();
        }
    }

    struct repeating_timer timer;
    add_repeating_timer_ms(-PERIOD_MS, sample_cb, NULL, &timer);

    print_help();
    leds_show(g_label + 1u);

    bool was_connected = stdio_usb_connected();

    while (true) {
        /* A terminal opened after power-up missed the banner: repeat it. */
        const bool connected = stdio_usb_connected();
        if (connected && !was_connected) {
            sleep_ms(200);
            print_help();
        }
        was_connected = connected;

        const int c = getchar_timeout_us(0);
        if (c != PICO_ERROR_TIMEOUT) {
            handle_command(c);
        }

        if (g_btn_label) {
            g_btn_label = false;
            if (g_recording) {
                continue;           /* the label is fixed once recording */
            }
            g_label = (g_label + 1u) % LABEL_COUNT;
            leds_show(g_label + 1u);
            printf("label is now: %s\n", LABELS[g_label]);
        }

        if (g_btn_record) {
            if (g_saved >= STORE_SLOTS) {
                printf("flash is full -- type d to print, then e to erase\n");
                beep(3);
            } else {
                printf("recording %s in 1 s ...\n", LABELS[g_label]);
                beep(1);
                sleep_ms(GET_READY_MS - 160);   /* the beep took 160 ms */
                g_index = 0;
                g_dropped = 0;
                leds_show(LED_COUNT);
                g_recording = true;
            }
            g_btn_record = false;   /* ignore presses during the countdown */
        }

        if (g_window_ready) {
            g_window_ready = false;
            g_rec.w.magic = STORE_MAGIC;
            g_rec.w.label = g_label;
            g_rec.w.dropped = g_dropped;
            print_window(&g_rec.w);
            if (save_window()) {
                printf("saved (%u in flash)\n", (unsigned)g_saved);
                beep(2);
            } else {
                printf("NOT saved -- flash full or write failed\n");
                beep(3);
            }
            g_btn_record = false;   /* nor during the recording */
            leds_show(g_label + 1u);
        }

        tight_loop_contents();
    }

    return 0;
}
