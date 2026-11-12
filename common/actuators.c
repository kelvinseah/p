/**
 * @file    actuators.c
 * @brief   Implementation of the buzzer and servo PWM helpers.
 */
#include "actuators.h"

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"

/** Smallest legal integer part of the RP2040 clock divider. */
#define CLKDIV_MIN 1.0f
/** Largest legal integer part of the RP2040 clock divider. */
#define CLKDIV_MAX 255.0f

/**
 * @brief Clamp a float to a closed interval.
 */
static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

void tone_play(uint gpio, float freq_hz, float duty)
{
    gpio_set_function(gpio, GPIO_FUNC_PWM);

    const uint slice = pwm_gpio_to_slice_num(gpio);

    /* Read the system clock rather than assuming 125 MHz -- the value is
     * configurable, and hard-coding it is how tones end up off-pitch. */
    const float f_sys = (float)clock_get_hz(clk_sys);

    /* Float division, deliberately. Truncating this to an integer puts a
     * 4 kHz tone 9 % sharp. */
    const float div = clampf(f_sys / (freq_hz * (TONE_WRAP + 1u)),
                             CLKDIV_MIN, CLKDIV_MAX);

    pwm_set_clkdiv(slice, div);
    pwm_set_wrap(slice, TONE_WRAP);
    pwm_set_gpio_level(gpio, (uint16_t)(clampf(duty, 0.0f, 1.0f) * TONE_WRAP));
    pwm_set_enabled(slice, true);
}

void tone_stop(uint gpio)
{
    pwm_set_gpio_level(gpio, 0);
    pwm_set_enabled(pwm_gpio_to_slice_num(gpio), false);

    /* Hand the pin back to the GPIO block and drive it low, so the buzzer
     * element is not left with a static voltage across it. */
    gpio_set_function(gpio, GPIO_FUNC_SIO);
    gpio_set_dir(gpio, GPIO_OUT);
    gpio_put(gpio, 0);
}

void servo_init(uint gpio)
{
    gpio_set_function(gpio, GPIO_FUNC_PWM);

    const uint slice = pwm_gpio_to_slice_num(gpio);

    /* One count == one microsecond. After this, wrap is the period in us and
     * level is the pulse width in us, so the datasheet numbers go straight in
     * with no arithmetic. */
    const float div = (float)clock_get_hz(clk_sys) / 1000000.0f;

    pwm_set_clkdiv(slice, clampf(div, CLKDIV_MIN, CLKDIV_MAX));
    pwm_set_wrap(slice, SERVO_PERIOD_US - 1u);
    pwm_set_enabled(slice, true);

    servo_write_deg(gpio, 90.0f);
}

void servo_write_us(uint gpio, uint16_t us)
{
    if (us < SERVO_MIN_US) {
        us = SERVO_MIN_US;
    } else if (us > SERVO_MAX_US) {
        us = SERVO_MAX_US;
    }
    pwm_set_gpio_level(gpio, us);
}

void servo_write_deg(uint gpio, float degrees)
{
    degrees = clampf(degrees, 0.0f, 180.0f);

    const float span = (float)(SERVO_MAX_US - SERVO_MIN_US);
    servo_write_us(gpio,
                   (uint16_t)((float)SERVO_MIN_US + degrees * span / 180.0f));
}
