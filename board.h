/**
 * @file    board.h
 * @brief   Pin map for the Robo Pico carrier board + Raspberry Pi Pico WH.
 *
 * One place for every pin number used in ENG1104. If a board revision moves
 * a pin, it is changed here and nowhere else.
 *
 * @note GP4 and GP5 are shared: they drive two of the indicator LEDs *and*
 *       carry the I2C bus on Grove 3. While a program still needs the bus,
 *       those two LEDs must be left alone -- use ::BOARD_LED_PINS_NO_I2C
 *       instead of ::BOARD_LED_PINS. (A one-shot diagnostic that has already
 *       finished with I2C, such as the Week 6 self-test, may take them back.)
 *
 * This header is a pure pin table: it depends only on pico/stdlib.h, so it
 * can be copied into any project. A file that actually uses ::BOARD_I2C or
 * ::BOARD_OLED_I2C must include "hardware/i2c.h" itself and link
 * hardware_i2c.
 */
#ifndef ENG1104_BOARD_H
#define ENG1104_BOARD_H

#include "pico/stdlib.h"

/** @name Indicator LEDs
 *  Nine blue LEDs, listed in the order they appear on the board -- which is
 *  deliberately not the numeric order of the GPIO numbers.
 *  @{ */
#define BOARD_LED_PINS      { 2, 3, 4, 5, 16, 17, 6, 26, 27 }
#define BOARD_LED_COUNT     9

/** LED list with the two I2C pins (GP4, GP5) removed. */
#define BOARD_LED_PINS_NO_I2C   { 2, 3, 16, 17, 6, 26, 27 }
#define BOARD_LED_COUNT_NO_I2C  7
/** @} */

/** @name User buttons
 *  Active low with an internal pull-up: idle reads 1, pressed reads 0.
 *  @{ */
#define BOARD_BTN_A         20u
#define BOARD_BTN_B         21u
/** @} */

/** Passive buzzer. Digital on/off in Week 8, PWM tones from Week 9. */
#define BOARD_BUZZER        22u

/** @name Servo headers (PWM capable) @{ */
#define BOARD_SERVO_0       12u
#define BOARD_SERVO_1       13u
#define BOARD_SERVO_2       14u
#define BOARD_SERVO_3       15u
/** @} */

/**
 * @name Sensor bus -- Grove port 3 and the GPIO breakout header
 *
 * The Robo Pico wires the Grove 3 socket and the GPIO breakout header to the
 * same GP4 / GP5 net, so both motion sensors sit on one I2C controller with
 * no hub and no address clash:
 *
 * - the GY-511, a 2.54 mm pin-header module, on the breakout at 0x19;
 * - the Grove 6-axis IMU, a native Grove module, in Grove 3 at 0x6A.
 *
 * @warning GP4 and GP5 are also LED pins. Once either sensor is connected,
 *          use ::BOARD_LED_PINS_NO_I2C instead of the full LED list.
 * @{ */
#define BOARD_I2C           i2c0
#define BOARD_I2C_SDA       4u
#define BOARD_I2C_SCL       5u
#define BOARD_I2C_BAUD      400000u   /**< 400 kHz fast mode. */
/** @} */

/** GY-511 (LSM303DLHC) accelerometer 7-bit I2C address. */
#define BOARD_GY511_ADDR    0x19

/**
 * Grove 6-axis IMU (LSM6DS3) 7-bit I2C address, one per project group.
 *
 * @note 0x6A with SA0 low. If an I2C scan reports 0x6B, the module ties SA0
 *       high -- change this and ::LSM6DS3_ADDR to match.
 */
#define BOARD_IMU_ADDR      0x6A

/**
 * @name Display bus -- Grove port 6, on the *second* I2C controller
 *
 * This is not an arbitrary choice. A full 128x64 frame is 1024 bytes, which
 * at 400 kHz takes about 23 ms -- longer than the 20 ms sampling period. Put
 * the screen on the same bus as the accelerometer and every redraw pushes a
 * sample late, which is exactly the jitter Week 9 is spent eliminating. The
 * RP2040 has two independent I2C controllers so that this never has to
 * happen.
 *
 * @warning Grove 5 shares GP26 with Grove 6, so Grove 5 is unusable while
 *          the display is connected.
 * @{ */
#define BOARD_OLED_I2C      i2c1
#define BOARD_OLED_SDA      26u
#define BOARD_OLED_SCL      27u
#define BOARD_OLED_BAUD     400000u
#define BOARD_OLED_ADDR     0x3C    /**< SSD1306, 7-bit. */
/** @} */

/**
 * @name Ultrasonic ranger -- Grove port 7
 *
 * One pin does both jobs: the program drives a 10 us trigger pulse, then
 * turns the pin around and measures how long the module holds it high.
 *
 * Grove 7 carries GP7 and GP28; the Robo Pico datasheet and silkscreen
 * disagree on which is pin 1 (SIG). Measured on real hardware on
 * 2 Oct 2026: the ranger answers on @b GP28.
 * @{ */
#define BOARD_SONAR_SIG     28u
/** @} */

/*
 * Lab Assessment 2 and the Final Project use no extra button module: the
 * alarm is cleared with the onboard GP20 button, so wear the board
 * face-out where that button can be reached.
 */

#endif /* ENG1104_BOARD_H */
