/**
 * @file    pwm_buzzer.c
 * @brief   ENG1104 Week 9 Ex03 -- PWM tones on the buzzer.
 *
 * In Week 8 the buzzer was switched on and off with @c gpio_put(), which
 * gives exactly one sound. Driving it from a PWM slice instead makes the
 * frequency -- the pitch -- a parameter, and the duty cycle controls how loud
 * it seems.
 *
 * Lab Assessment 2 asks for two distinguishable alarm tones, which is why
 * this matters.
 *
 * Plays a short scale, then two alarm patterns of the kind the fall detector
 * will use.
 */
#include <stdio.h>

#include "actuators.h"
#include "board.h"
#include "pico/stdlib.h"

/** Note frequencies, in hertz. */
#define NOTE_A4  440.0f
#define NOTE_C5  523.0f
#define NOTE_E5  659.0f
#define NOTE_A5  880.0f

/**
 * @brief Play one note for a fixed time, then silence.
 *
 * @param freq_hz  Pitch in hertz.
 * @param ms       Duration in milliseconds.
 * @param duty     Duty cycle, 0.0 to 1.0.
 */
static void note(float freq_hz, uint32_t ms, float duty)
{
    tone_play(BOARD_BUZZER, freq_hz, duty);
    sleep_ms(ms);
    tone_stop(BOARD_BUZZER);
}

/**
 * @brief Alarm pattern 1 -- a fast two-tone warble.
 */
static void alarm_warble(uint32_t cycles)
{
    /* TODO(student): Design alarm pattern 1. It must be audible and clearly
         distinguishable from pattern 2 across a noisy lab.
     */
}

/**
 * @brief Alarm pattern 2 -- slow, low, clearly different from pattern 1.
 */
static void alarm_slow(uint32_t cycles)
{
    /* TODO(student): Design alarm pattern 2. Different pitch AND different rhythm
         from pattern 1.
     */
}

int main(void)
{
    stdio_init_all();
    sleep_ms(2500);

    printf("\nENG1104 W09 Ex03 -- PWM buzzer\n");
    printf("buzzer on GP%u, PWM slice shared with GP23\n\n",
           (unsigned)BOARD_BUZZER);

    while (true) {
        printf("scale\n");
        note(NOTE_A4, 250, 0.5f);
        note(NOTE_C5, 250, 0.5f);
        note(NOTE_E5, 250, 0.5f);
        note(NOTE_A5, 400, 0.5f);
        sleep_ms(600);

        printf("duty sweep at %.0f Hz -- listen to the loudness change\n",
               (double)NOTE_C5);
        for (float duty = 0.05f; duty <= 0.5f; duty += 0.05f) {
            tone_play(BOARD_BUZZER, NOTE_C5, duty);
            sleep_ms(150);
        }
        tone_stop(BOARD_BUZZER);
        sleep_ms(600);

        printf("alarm pattern 1 (warble)\n");
        alarm_warble(4);
        sleep_ms(800);

        printf("alarm pattern 2 (slow)\n");
        alarm_slow(3);
        sleep_ms(1500);
    }

    return 0;
}
