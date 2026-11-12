/**
 * @file    actuators.h
 * @brief   PWM helpers for the buzzer and the servo motor.
 * @author  ENG1104 teaching team
 *
 * @section act_pwm How PWM works on the RP2040
 *
 * The RP2040 has eight PWM @e slices, each driving two channels (A and B),
 * for sixteen outputs in total. Every slice contains a counter that runs from
 * 0 up to a @e wrap value and then restarts. The output is high while the
 * counter is below the channel's @e level and low above it, so:
 *
 * @code
 *     f_pwm = f_sys / (clkdiv * (wrap + 1))
 *     duty  = level / (wrap + 1)
 * @endcode
 *
 * @warning @c clkdiv and @c wrap belong to the @b slice, not the channel. Two
 * GPIOs on the same slice therefore share a frequency and can only differ in
 * duty cycle. GP12/GP13 are one slice and GP14/GP15 are another, so two servos
 * on GP12 and GP14 are independent -- but GP12 and GP13 are not.
 *
 * @section act_ranges Choosing clkdiv and wrap
 *
 * @c clkdiv is an 8.4 fixed-point number, so its integer part is limited to
 * 1..255. Truncating it to an integer is a real bug: at 4 kHz the exact
 * divider is 7.63, and rounding it down to 7 puts the tone 9 % sharp.
 * ::tone_play therefore computes it as a float.
 *
 * For the servo a different trick is used. Setting @c clkdiv so the counter
 * ticks at exactly 1 MHz makes one count equal one microsecond, after which
 * @c wrap is the period in microseconds and @c level is the pulse width in
 * microseconds. The datasheet numbers can then be used directly.
 */
#ifndef ENG1104_ACTUATORS_H
#define ENG1104_ACTUATORS_H

#include <stdbool.h>
#include <stdint.h>

#include "pico/stdlib.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Counter resolution used for buzzer tones. */
#define TONE_WRAP           4095u

/** @name Servo timing (standard hobby servo) @{ */
#define SERVO_PERIOD_US     20000u  /**< 20 ms period, i.e. 50 Hz. */
#define SERVO_MIN_US        500u    /**< Pulse width at 0 degrees. */
#define SERVO_MAX_US        2500u   /**< Pulse width at 180 degrees. */
/** @} */

/**
 * @brief Start a tone on a GPIO, or change one that is already playing.
 *
 * Usable range with ::TONE_WRAP is roughly 120 Hz to 30 kHz; requests outside
 * it are clamped to the nearest achievable frequency.
 *
 * @param gpio     GPIO to drive. The buzzer is on GP22.
 * @param freq_hz  Tone frequency in hertz.
 * @param duty     Duty cycle, 0.0 to 1.0. Use 0.5 for a square wave; lower
 *                 values sound quieter on a passive buzzer.
 */
void tone_play(uint gpio, float freq_hz, float duty);

/**
 * @brief Silence a tone and leave the pin low.
 *
 * @param gpio  GPIO previously passed to ::tone_play.
 */
void tone_stop(uint gpio);

/**
 * @brief Configure a GPIO to drive a standard hobby servo at 50 Hz.
 *
 * Must be called once before ::servo_write_us or ::servo_write_deg. The
 * output is left at the centre position.
 *
 * @param gpio  Servo signal pin. The carrier board exposes GP12..GP15.
 */
void servo_init(uint gpio);

/**
 * @brief Command a servo by pulse width.
 *
 * @param gpio  Pin previously passed to ::servo_init.
 * @param us    Pulse width in microseconds, clamped to
 *              ::SERVO_MIN_US .. ::SERVO_MAX_US.
 */
void servo_write_us(uint gpio, uint16_t us);

/**
 * @brief Command a servo by angle.
 *
 * @param gpio     Pin previously passed to ::servo_init.
 * @param degrees  0.0 to 180.0. Values outside the range are clamped.
 *
 * @note A servo needs time to travel -- roughly 0.1 s per 60 degrees for the
 *       motors in the kit. Commanding a new angle and immediately reading a
 *       sensor will catch the arm still moving.
 */
void servo_write_deg(uint gpio, float degrees);

#ifdef __cplusplus
}
#endif

#endif /* ENG1104_ACTUATORS_H */
