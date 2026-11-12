/**
 * @file    ssd1306.h
 * @brief   Minimal text driver for the 0.96" SSD1306 OLED, over I2C.
 * @author  ENG1104 teaching team
 *
 * This is a tool, not an exercise. It exists so that a board running on
 * battery -- with no USB and therefore no `printf` -- can still show you what
 * it is doing. You are not asked to write it; you are asked to use it
 * correctly.
 *
 * @section ssd1306_bus Which bus, and why it matters
 *
 * The display is wired to @b Grove @b 6 (GP26 / GP27), which is the RP2040's
 * @b second I2C controller. That is deliberate.
 *
 * A full 128x64 frame is 1024 bytes. Every byte on the wire costs 9 clock
 * periods (8 data bits plus the ACK), so at 400 kHz a whole frame takes
 *
 *     1024 x 9 / 400000 = 23 ms
 *
 * which is @b longer @b than @b the @b 20 @b ms @b sampling @b period. On a
 * shared bus, each redraw would push an accelerometer sample late -- exactly
 * the jitter Week 9 is spent eliminating. Two controllers means the sampler
 * and the screen never queue behind one another.
 *
 * @section ssd1306_partial Never redraw the whole screen
 *
 * Even on its own bus, a 23 ms transfer is a long time to spend. The driver
 * keeps a 1 KB frame buffer in RAM and tracks which of the eight 8-pixel
 * pages have actually changed. ::ssd1306_flush sends only those pages, so
 * updating one line of text costs 128 bytes (about 2.9 ms) instead of 1024.
 *
 * @section ssd1306_isr Where you may call it
 *
 * @warning Call these functions from the @b main @b loop @b only. Never from
 *          a timer callback or a GPIO interrupt handler. I2C transfers block
 *          for milliseconds, and an ISR that blocks for milliseconds is no
 *          longer an ISR.
 *
 * @section ssd1306_font What it can print
 *
 * A 5x7 font covering ASCII 0x20 to 0x5F: space, punctuation, digits and
 * uppercase letters. Lowercase input is converted to uppercase rather than
 * dropped. Each glyph occupies 6 columns including the gap, giving 21
 * characters per line and 8 lines of text.
 */
#ifndef ENG1104_SSD1306_H
#define ENG1104_SSD1306_H

#include <stdbool.h>
#include <stdint.h>

#include "hardware/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Display width in pixels. */
#define SSD1306_WIDTH       128
/** Display height in pixels. */
#define SSD1306_HEIGHT      64
/** Text rows: one per 8-pixel page. */
#define SSD1306_ROWS        8
/** Text columns: 128 pixels / 6 pixels per character. */
#define SSD1306_COLS        21

/**
 * @brief  Configure the panel and clear it.
 *
 * Brings up the charge pump and puts the controller in page-addressing mode,
 * which is what makes single-page updates possible. Also initialises the I2C
 * peripheral and its two pins.
 *
 * @param  i2c   I2C instance -- pass ::BOARD_OLED_I2C, not the sensor's bus.
 * @param  sda   SDA pin number.
 * @param  scl   SCL pin number.
 * @param  addr  7-bit address, normally ::BOARD_OLED_ADDR (0x3C).
 * @return true if the panel acknowledged its address, false if nothing
 *         answered -- check the cable and the VCC/GND leads before anything
 *         else.
 */
bool ssd1306_init(i2c_inst_t *i2c, uint sda, uint scl, uint8_t addr);

/** @brief Blank the frame buffer and mark every page dirty. */
void ssd1306_clear(void);

/**
 * @brief  Draw text into the frame buffer. Nothing reaches the panel yet.
 *
 * Characters that fall off the right-hand edge are dropped rather than
 * wrapped, so a long string cannot corrupt the row below it.
 *
 * @param  row   Text row, 0 (top) to ::SSD1306_ROWS - 1.
 * @param  col   Text column, 0 (left) to ::SSD1306_COLS - 1.
 * @param  text  NUL-terminated string.
 */
void ssd1306_write(uint8_t row, uint8_t col, const char *text);

/**
 * @brief  Clear one text row in the frame buffer.
 * @param  row  Text row to blank.
 */
void ssd1306_clear_row(uint8_t row);

/**
 * @brief  Send only the pages that changed since the last flush.
 * @return Number of pages actually transmitted, 0 to 8. Print it if you want
 *         to see the cost of a redraw for yourself.
 */
int ssd1306_flush(void);

/** @brief Force all eight pages onto the wire. Costs the full 23 ms. */
int ssd1306_flush_all(void);

#ifdef __cplusplus
}
#endif

#endif /* ENG1104_SSD1306_H */
