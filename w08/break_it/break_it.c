/**
 * @file    break_it.c
 * @brief   ENG1104 Week 8 Ex03 -- break the accelerometer on purpose.
 *
 * The lecture warned about two mistakes that silently ruin accelerometer
 * data. This program lets you make each one by changing a single number,
 * rebuild, and see the result for yourself.
 *
 * The program talks to the GY-511 directly with i2c_write_blocking() and
 * i2c_read_blocking() -- it does not use the gy511 driver -- so every byte
 * that goes over the wire is visible in this file.
 *
 *   USE_AUTO_INC  1 = correct.   0 = forget bit 7 of the sub-address
 *                                   (lecture: "the auto-increment trap")
 *   RANGE_8G      1 = correct.   0 = leave the power-on default of +/-2 g
 *                                   (lecture: "why +/-2 g is not enough")
 *
 * Change one switch at a time, rebuild, flash, and compare with the correct
 * output.
 *
 * If your OLED is connected to Grove 6, the current reading and the peak are
 * also shown live on the screen. Press GP20 to reset the peak.
 */
#include <stdio.h>

#include "board.h"
#include "hardware/i2c.h"
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"
#include "ssd1306.h"

/* ====================================================================== */
/*  The two switches. Change ONE at a time.                               */
/* ====================================================================== */
#define USE_AUTO_INC    1
#define RANGE_8G        1
/* ====================================================================== */

#define GY511_ADDR      0x19u
#define CTRL_REG1_A     0x20u
#define CTRL_REG4_A     0x23u
#define OUT_X_L_A       0x28u
#define AUTO_INC_BIT    0x80u

/** Block until a serial monitor has the port open -- see Week 6 Ex02. */
static void wait_for_serial_monitor(void)
{
    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }
    sleep_ms(200);
}

static void write_reg(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = { reg, value };
    i2c_write_blocking(BOARD_I2C, GY511_ADDR, buf, 2, false);
}

/**
 * @brief Read the six output bytes, X low byte first.
 * @param out  Six bytes: XL XH YL YH ZL ZH.
 */
static void read_six(uint8_t out[6])
{
    uint8_t sub = OUT_X_L_A;
#if USE_AUTO_INC
    sub |= AUTO_INC_BIT;       /* ask the sensor to advance after each byte */
#endif
    i2c_write_blocking(BOARD_I2C, GY511_ADDR, &sub, 1, true);
    i2c_read_blocking(BOARD_I2C, GY511_ADDR, out, 6, false);
}

int main(void)
{
    stdio_init_all();
    wait_for_serial_monitor();

    i2c_init(BOARD_I2C, BOARD_I2C_BAUD);
    gpio_set_function(BOARD_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(BOARD_I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(BOARD_I2C_SDA);
    gpio_pull_up(BOARD_I2C_SCL);

    write_reg(CTRL_REG1_A, 0x47);              /* 50 Hz, X Y Z on */
#if RANGE_8G
    write_reg(CTRL_REG4_A, 0xA8);              /* +/-8 g, high res, BDU */
    const float g_per_count = 0.004f;          /* 4 mg per count at +/-8 g */
#else
    write_reg(CTRL_REG4_A, 0x88);              /* +/-2 g, high res, BDU */
    const float g_per_count = 0.001f;          /* 1 mg per count at +/-2 g */
#endif

    printf("\nENG1104 W08 Ex03 -- break it on purpose\n");
    printf("USE_AUTO_INC = %d   RANGE_8G = %d\n\n", USE_AUTO_INC, RANGE_8G);

    /* Optional: the OLED on Grove 6 shows the same numbers live. */
    bool screen = ssd1306_init(BOARD_OLED_I2C, BOARD_OLED_SDA, BOARD_OLED_SCL,
                               BOARD_OLED_ADDR);
    gpio_init(BOARD_BTN_A);
    gpio_set_dir(BOARD_BTN_A, GPIO_IN);
    gpio_pull_up(BOARD_BTN_A);

    float peak = 0.0f;
    uint32_t n = 0;

    while (true) {
        uint8_t b[6];
        read_six(b);

        int16_t x = (int16_t)(b[1] << 8 | b[0]) >> 4;
        int16_t y = (int16_t)(b[3] << 8 | b[2]) >> 4;
        int16_t z = (int16_t)(b[5] << 8 | b[4]) >> 4;
        float gx = x * g_per_count, gy = y * g_per_count, gz = z * g_per_count;

        float biggest = gx < 0 ? -gx : gx;
        if ((gy < 0 ? -gy : gy) > biggest) biggest = gy < 0 ? -gy : gy;
        if ((gz < 0 ? -gz : gz) > biggest) biggest = gz < 0 ? -gz : gz;
        if (biggest > peak) peak = biggest;
        if (gpio_get(BOARD_BTN_A) == 0) {
            peak = 0.0f;                       /* GP20 pressed: start again */
        }

        /* Read at 50 Hz so a short impact is not missed, but print only
         * every 10th sample so the text stays readable. */
        if ((n++ % 10u) == 0u) {
            printf("bytes %02X %02X %02X %02X %02X %02X   "
                   "g %6.2f %6.2f %6.2f   peak %5.2f g\n",
                   b[0], b[1], b[2], b[3], b[4], b[5],
                   (double)gx, (double)gy, (double)gz, (double)peak);
            if (screen) {
                char line[SSD1306_COLS + 1];
                snprintf(line, sizeof line, "RANGE +/-%dG  INC %d",
                         RANGE_8G ? 8 : 2, USE_AUTO_INC);
                ssd1306_write(0, 0, line);
                snprintf(line, sizeof line, "X    %6.2f G", (double)gx);
                ssd1306_write(2, 0, line);
                snprintf(line, sizeof line, "Y    %6.2f G", (double)gy);
                ssd1306_write(3, 0, line);
                snprintf(line, sizeof line, "Z    %6.2f G", (double)gz);
                ssd1306_write(4, 0, line);
                snprintf(line, sizeof line, "PEAK %6.2f G", (double)peak);
                ssd1306_write(6, 0, line);
                ssd1306_flush();
            }
        }
        sleep_ms(20);
    }
}
