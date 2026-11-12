/**
 * @file    gpio_irq.c
 * @brief   ENG1104 Week 9 Ex01 -- GPIO interrupts versus polling.
 *
 * In Week 6 the buttons were polled, and a press made during a @c sleep_ms()
 * was simply lost. This program shows the alternative: the hardware notices
 * the edge and calls a function of yours, whatever the main loop happens to
 * be doing.
 *
 * To make the difference obvious, the main loop deliberately wastes 500 ms at
 * a time. Press the buttons during that pause:
 *
 *   - GP20 is polled  -> presses during the pause are missed
 *   - GP21 is on an interrupt -> every press is counted
 *
 * @section isr_rules What an interrupt handler may do
 *
 * The handler runs with the main program suspended, so it must be short.
 * The rules used throughout this module:
 *
 *   - set a flag or push to a buffer, and return
 *   - never @c printf() from an ISR -- it is slow and can deadlock
 *   - never @c sleep_ms() or block
 *   - anything the ISR and @c main() share must be @c volatile, or the
 *     compiler may cache it in a register and never see the change
 */
#include <stdio.h>

#include "board.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

/** Debounce window applied inside the ISR, in microseconds. */
#define DEBOUNCE_US 200000u

/** Presses seen by the interrupt handler. Shared with main() -> volatile. */
static volatile uint32_t g_irq_presses = 0;
/** Timestamp of the last accepted interrupt, for debouncing. */
static volatile uint64_t g_last_irq_us = 0;

/** Presses seen by the polling loop. Only main() touches this. */
static uint32_t g_poll_presses = 0;

/**
 * @brief GPIO interrupt handler.
 *
 * Registered for one pin here, but the SDK passes the pin number and the
 * event mask so a single handler can serve many pins.
 *
 * @param gpio    GPIO that caused the interrupt.
 * @param events  Bit mask of which edge or level fired.
 */
static void gpio_callback(uint gpio, uint32_t events)
{
    /* TODO(student): Debounce against g_last_irq_us (DEBOUNCE_US), then increment
         g_irq_presses. Keep it SHORT -- no printf, no sleep.
     */
}

int main(void)
{
    stdio_init_all();
    sleep_ms(2500);

    gpio_init(BOARD_BTN_A);
    gpio_set_dir(BOARD_BTN_A, GPIO_IN);
    gpio_pull_up(BOARD_BTN_A);

    gpio_init(BOARD_BTN_B);
    gpio_set_dir(BOARD_BTN_B, GPIO_IN);
    gpio_pull_up(BOARD_BTN_B);

    /* Falling edge, because the buttons are active low. */
    gpio_set_irq_enabled_with_callback(BOARD_BTN_B, GPIO_IRQ_EDGE_FALL, true,
                                       &gpio_callback);

    printf("\nENG1104 W09 Ex01 -- interrupts vs polling\n");
    printf("GP%u is polled, GP%u is on an interrupt.\n",
           (unsigned)BOARD_BTN_A, (unsigned)BOARD_BTN_B);
    printf("Press both during the 500 ms pause and compare the counts.\n\n");

    uint32_t last_reported_poll = 0;
    uint32_t last_reported_irq = 0;

    while (true) {
        /* One single poll, then a long blind spot -- exactly the shape of a
         * main loop that has real work to do. */
        if (gpio_get(BOARD_BTN_A) == 0) {
            g_poll_presses++;
        }

        sleep_ms(500);   /* the blind spot */

        uint32_t irq = g_irq_presses;   /* one read of the volatile */

        if (irq != last_reported_irq || g_poll_presses != last_reported_poll) {
            printf("polled(GP%u) = %-4u   interrupt(GP%u) = %-4u\n",
                   (unsigned)BOARD_BTN_A, (unsigned)g_poll_presses,
                   (unsigned)BOARD_BTN_B, (unsigned)irq);
            last_reported_poll = g_poll_presses;
            last_reported_irq = irq;
        }
    }

    return 0;
}
