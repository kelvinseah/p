/**
 * @file    servo.c
 * @brief   ENG1104 Week 9 Ex04 -- servo control, the Final Project clamp.
 *
 * A standard hobby servo is told where to go by the @e width of a pulse
 * repeated every 20 ms:
 *
 * @code
 *   |<------------------ 20 ms (50 Hz) ------------------>|
 *    ___                                                   ___
 *   |   |_________________________________________________|   |___ ...
 *   |<->|
 *   500 us  = 0 degrees      1500 us = 90 degrees (centre)
 *   2500 us = 180 degrees
 * @endcode
 *
 * Only the pulse width carries information; the 20 ms period is just the
 * refresh rate the servo expects. ::servo_init sets the PWM counter to tick
 * once per microsecond, so the level register @e is the pulse width in
 * microseconds.
 *
 * @section servo_wiring Wiring
 *
 * Orange (or white) = signal, red = +V, brown (or black) = ground. Getting
 * red and brown the wrong way round destroys the servo.
 *
 * @warning A servo can draw far more current than the USB port comfortably
 *          supplies, especially when it stalls. If the board resets whenever
 *          the servo moves, that is a power problem, not a code problem --
 *          power the servo from the battery pack.
 */
#include <stdio.h>

#include "actuators.h"
#include "board.h"
#include "pico/stdlib.h"

/** Servo signal pin. GP12..GP15 are available on the carrier board. */
#define SERVO_PIN BOARD_SERVO_0

/** Clamp positions for the Final Project release mechanism. */
#define CLAMP_CLOSED_DEG    20.0f
#define CLAMP_OPEN_DEG      110.0f

/** Time to allow the arm to actually get there, in milliseconds. */
#define SETTLE_MS           400u

int main(void)
{
    stdio_init_all();
    sleep_ms(2500);

    printf("\nENG1104 W09 Ex04 -- servo\n");
    printf("signal on GP%u, %u us .. %u us, %u ms period\n\n",
           (unsigned)SERVO_PIN, (unsigned)SERVO_MIN_US,
           (unsigned)SERVO_MAX_US, (unsigned)(SERVO_PERIOD_US / 1000u));

    servo_init(SERVO_PIN);
    sleep_ms(SETTLE_MS);

    while (true) {
        /* --- 1. named positions ------------------------------------- */
        printf("endpoints: 0, 90, 180 degrees\n");
        for (float deg = 0.0f; deg <= 180.0f; deg += 90.0f) {
            servo_write_deg(SERVO_PIN, deg);
            printf("  %5.1f deg\n", (double)deg);
            sleep_ms(SETTLE_MS + 300u);
        }

        /* --- 2. smooth sweep ---------------------------------------- */
        printf("sweep\n");
        for (float deg = 0.0f; deg <= 180.0f; deg += 2.0f) {
            servo_write_deg(SERVO_PIN, deg);
            sleep_ms(15);
        }
        for (float deg = 180.0f; deg >= 0.0f; deg -= 2.0f) {
            servo_write_deg(SERVO_PIN, deg);
            sleep_ms(15);
        }

        /* --- 3. the clamp gesture ----------------------------------- */
        printf("clamp: close, hold, release\n");
        servo_write_deg(SERVO_PIN, CLAMP_CLOSED_DEG);
        sleep_ms(SETTLE_MS);
        sleep_ms(1000);

        uint64_t t0 = time_us_64();
        servo_write_deg(SERVO_PIN, CLAMP_OPEN_DEG);
        printf("  release commanded at t = %llu us\n",
               (unsigned long long)t0);
        sleep_ms(SETTLE_MS);

        printf("  (the command is instant; the ARM takes ~%u ms to travel --\n",
               (unsigned)SETTLE_MS);
        printf("   that delay is what the Final Project has to compensate)\n\n");
        sleep_ms(1500);
    }

    return 0;
}
