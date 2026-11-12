/**
 * @file    oled.c
 * @brief   ENG1104 Week 8 Ex05 (optional) -- your readings on the OLED.
 *
 * Two I2C devices on two separate buses:
 *
 *   GY-511 accelerometer  Grove 3  i2c0  GP4 / GP5    address 0x19
 *   OLED display          Grove 6  i2c1  GP26 / GP27  address 0x3C
 *
 * The program reads the accelerometer and shows X, Y, Z and |a| on the
 * screen five times a second. The font has capital letters, digits and a
 * little punctuation only -- no lower case and no "|" -- so any
 * lower-case text you write is shown in capitals.
 *
 * The top text row of this panel is yellow, the rest blue -- that is the
 * glass, not the code. Row 0 is a good place for a title.
 */
#include <stdio.h>

#include "board.h"
#include "gy511.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "ssd1306.h"

/**
 * @brief Put one sample on the screen.
 *
 * Writes into the frame buffer row by row, then sends it with
 * ssd1306_flush(), which only transmits the rows that changed.
 *
 * @param g    Acceleration in g.
 * @param mag  Its magnitude in g.
 */
static void show_on_oled(const gy511_g_t *g, float mag)
{
    char line[SSD1306_COLS + 1];

    ssd1306_write(0, 0, "ENG1104 ACCEL");
    snprintf(line, sizeof line, "X %6.2f G", (double)g->x);
    ssd1306_write(2, 0, line);
    snprintf(line, sizeof line, "Y %6.2f G", (double)g->y);
    ssd1306_write(3, 0, line);
    snprintf(line, sizeof line, "Z %6.2f G", (double)g->z);
    ssd1306_write(4, 0, line);
    snprintf(line, sizeof line, "MAG %5.2f G", (double)mag);
    ssd1306_write(6, 0, line);
    ssd1306_flush();
}

int main(void)
{
    stdio_init_all();

    i2c_init(BOARD_I2C, BOARD_I2C_BAUD);
    gpio_set_function(BOARD_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(BOARD_I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(BOARD_I2C_SDA);
    gpio_pull_up(BOARD_I2C_SCL);

    bool sensor_ok = gy511_init(BOARD_I2C);
    bool screen_ok = ssd1306_init(BOARD_OLED_I2C, BOARD_OLED_SDA,
                                  BOARD_OLED_SCL, BOARD_OLED_ADDR);
    printf("gy511: %s   oled: %s\n", sensor_ok ? "OK" : "FAILED",
           screen_ok ? "OK" : "FAILED");

    while (true) {
        gy511_g_t g;
        if (screen_ok && gy511_read_g(BOARD_I2C, &g)) {
            show_on_oled(&g, gy511_magnitude(&g));
        }
        sleep_ms(200);
    }
}
