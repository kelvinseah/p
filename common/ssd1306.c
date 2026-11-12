/**
 * @file    ssd1306.c
 * @brief   Minimal text driver for the 0.96" SSD1306 OLED. See ssd1306.h.
 */
#include "ssd1306.h"

#include <string.h>

#include "hardware/gpio.h"

/** Control byte prefixing a command stream. */
#define CTRL_CMD    0x00u
/** Control byte prefixing a data stream. */
#define CTRL_DATA   0x40u

static i2c_inst_t *s_i2c;
static uint8_t     s_addr;

/** One byte per column per page: 8 pages x 128 columns. */
static uint8_t s_buf[SSD1306_ROWS][SSD1306_WIDTH];
/** Bit n set means page n has changed since the last flush. */
static uint8_t s_dirty;

/**
 * 5x7 font, ASCII 0x20 ('space') to 0x5F ('_').
 *
 * Each glyph is five column bytes; bit 0 is the top pixel row and bit 6 the
 * bottom. A sixth, blank column is added at draw time to separate glyphs.
 */
static const uint8_t FONT5X7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00},  /* 0x20 space */
    {0x00, 0x00, 0x5F, 0x00, 0x00},  /* !  */
    {0x00, 0x07, 0x00, 0x07, 0x00},  /* "  */
    {0x14, 0x7F, 0x14, 0x7F, 0x14},  /* #  */
    {0x24, 0x2A, 0x7F, 0x2A, 0x12},  /* $  */
    {0x23, 0x13, 0x08, 0x64, 0x62},  /* %  */
    {0x36, 0x49, 0x55, 0x22, 0x50},  /* &  */
    {0x00, 0x05, 0x03, 0x00, 0x00},  /* '  */
    {0x00, 0x1C, 0x22, 0x41, 0x00},  /* (  */
    {0x00, 0x41, 0x22, 0x1C, 0x00},  /* )  */
    {0x14, 0x08, 0x3E, 0x08, 0x14},  /* *  */
    {0x08, 0x08, 0x3E, 0x08, 0x08},  /* +  */
    {0x00, 0x50, 0x30, 0x00, 0x00},  /* ,  */
    {0x08, 0x08, 0x08, 0x08, 0x08},  /* -  */
    {0x00, 0x60, 0x60, 0x00, 0x00},  /* .  */
    {0x20, 0x10, 0x08, 0x04, 0x02},  /* /  */
    {0x3E, 0x51, 0x49, 0x45, 0x3E},  /* 0  */
    {0x00, 0x42, 0x7F, 0x40, 0x00},  /* 1  */
    {0x42, 0x61, 0x51, 0x49, 0x46},  /* 2  */
    {0x21, 0x41, 0x45, 0x4B, 0x31},  /* 3  */
    {0x18, 0x14, 0x12, 0x7F, 0x10},  /* 4  */
    {0x27, 0x45, 0x45, 0x45, 0x39},  /* 5  */
    {0x3C, 0x4A, 0x49, 0x49, 0x30},  /* 6  */
    {0x01, 0x71, 0x09, 0x05, 0x03},  /* 7  */
    {0x36, 0x49, 0x49, 0x49, 0x36},  /* 8  */
    {0x06, 0x49, 0x49, 0x29, 0x1E},  /* 9  */
    {0x00, 0x36, 0x36, 0x00, 0x00},  /* :  */
    {0x00, 0x56, 0x36, 0x00, 0x00},  /* ;  */
    {0x08, 0x14, 0x22, 0x41, 0x00},  /* <  */
    {0x14, 0x14, 0x14, 0x14, 0x14},  /* =  */
    {0x00, 0x41, 0x22, 0x14, 0x08},  /* >  */
    {0x02, 0x01, 0x51, 0x09, 0x06},  /* ?  */
    {0x32, 0x49, 0x79, 0x41, 0x3E},  /* @  */
    {0x7E, 0x11, 0x11, 0x11, 0x7E},  /* A  */
    {0x7F, 0x49, 0x49, 0x49, 0x36},  /* B  */
    {0x3E, 0x41, 0x41, 0x41, 0x22},  /* C  */
    {0x7F, 0x41, 0x41, 0x22, 0x1C},  /* D  */
    {0x7F, 0x49, 0x49, 0x49, 0x41},  /* E  */
    {0x7F, 0x09, 0x09, 0x09, 0x01},  /* F  */
    {0x3E, 0x41, 0x49, 0x49, 0x7A},  /* G  */
    {0x7F, 0x08, 0x08, 0x08, 0x7F},  /* H  */
    {0x00, 0x41, 0x7F, 0x41, 0x00},  /* I  */
    {0x20, 0x40, 0x41, 0x3F, 0x01},  /* J  */
    {0x7F, 0x08, 0x14, 0x22, 0x41},  /* K  */
    {0x7F, 0x40, 0x40, 0x40, 0x40},  /* L  */
    {0x7F, 0x02, 0x0C, 0x02, 0x7F},  /* M  */
    {0x7F, 0x04, 0x08, 0x10, 0x7F},  /* N  */
    {0x3E, 0x41, 0x41, 0x41, 0x3E},  /* O  */
    {0x7F, 0x09, 0x09, 0x09, 0x06},  /* P  */
    {0x3E, 0x41, 0x51, 0x21, 0x5E},  /* Q  */
    {0x7F, 0x09, 0x19, 0x29, 0x46},  /* R  */
    {0x46, 0x49, 0x49, 0x49, 0x31},  /* S  */
    {0x01, 0x01, 0x7F, 0x01, 0x01},  /* T  */
    {0x3F, 0x40, 0x40, 0x40, 0x3F},  /* U  */
    {0x1F, 0x20, 0x40, 0x20, 0x1F},  /* V  */
    {0x3F, 0x40, 0x38, 0x40, 0x3F},  /* W  */
    {0x63, 0x14, 0x08, 0x14, 0x63},  /* X  */
    {0x07, 0x08, 0x70, 0x08, 0x07},  /* Y  */
    {0x61, 0x51, 0x49, 0x45, 0x43},  /* Z  */
    {0x00, 0x7F, 0x41, 0x41, 0x00},  /* [  */
    {0x02, 0x04, 0x08, 0x10, 0x20},  /* \  */
    {0x00, 0x41, 0x41, 0x7F, 0x00},  /* ]  */
    {0x04, 0x02, 0x01, 0x02, 0x04},  /* ^  */
    {0x40, 0x40, 0x40, 0x40, 0x40},  /* _  */
};

#define FONT_FIRST  0x20
#define FONT_LAST   0x5F
#define GLYPH_W     6       /**< 5 columns of pixels plus a 1-column gap. */

/** Send a single command byte. */
static void cmd(uint8_t c)
{
    const uint8_t pkt[2] = {CTRL_CMD, c};
    i2c_write_blocking(s_i2c, s_addr, pkt, 2, false);
}

bool ssd1306_init(i2c_inst_t *i2c, uint sda, uint scl, uint8_t addr)
{
    s_i2c = i2c;
    s_addr = addr;

    i2c_init(s_i2c, 400000u);
    gpio_set_function(sda, GPIO_FUNC_I2C);
    gpio_set_function(scl, GPIO_FUNC_I2C);
    gpio_pull_up(sda);
    gpio_pull_up(scl);

    /* Does anything answer at this address? A zero-length write is the
       cheapest possible probe. */
    uint8_t probe = 0;
    if (i2c_write_blocking(s_i2c, s_addr, &probe, 1, false) < 0) {
        return false;
    }

    static const uint8_t INIT[] = {
        0xAE,               /* display off                                */
        0x20, 0x02,         /* PAGE addressing -- required for partial     */
                            /* updates: one page can be sent on its own.  */
        0xB0,               /* start at page 0                            */
        0xC8,               /* COM scan direction: remapped               */
        0x00, 0x10,         /* column start, low and high nibble          */
        0x40,              /* display start line 0                        */
        0x81, 0x9F,         /* contrast                                   */
        0xA1,               /* segment remap                              */
        0xA6,               /* normal, not inverted                       */
        0xA8, 0x3F,         /* multiplex ratio: 64 rows                   */
        0xA4,               /* follow RAM, not all-pixels-on              */
        0xD3, 0x00,         /* no display offset                          */
        0xD5, 0x80,         /* clock divide / oscillator frequency        */
        0xD9, 0xF1,         /* pre-charge period                          */
        0xDA, 0x12,         /* COM pin configuration for 128x64           */
        0xDB, 0x40,         /* VCOMH deselect level                       */
        0x8D, 0x14,         /* charge pump on -- without this the panel   */
                            /* initialises but stays dark                 */
        0xAF,               /* display on                                 */
    };
    for (size_t i = 0; i < sizeof INIT; i++) {
        cmd(INIT[i]);
    }

    ssd1306_clear();
    ssd1306_flush_all();
    return true;
}

void ssd1306_clear(void)
{
    memset(s_buf, 0, sizeof s_buf);
    s_dirty = 0xFF;
}

void ssd1306_clear_row(uint8_t row)
{
    if (row >= SSD1306_ROWS) {
        return;
    }
    memset(s_buf[row], 0, SSD1306_WIDTH);
    s_dirty |= (uint8_t)(1u << row);
}

void ssd1306_write(uint8_t row, uint8_t col, const char *text)
{
    if (row >= SSD1306_ROWS || text == NULL) {
        return;
    }

    for (const char *p = text; *p != '\0'; p++, col++) {
        if (col >= SSD1306_COLS) {
            break;              /* clip, never wrap into the next row */
        }

        char c = *p;
        if (c >= 'a' && c <= 'z') {
            c = (char)(c - 'a' + 'A');
        }
        if (c < FONT_FIRST || c > FONT_LAST) {
            c = '?';
        }

        const uint8_t *glyph = FONT5X7[(uint8_t)c - FONT_FIRST];
        int x = col * GLYPH_W;
        for (int i = 0; i < 5; i++) {
            s_buf[row][x + i] = glyph[i];
        }
        s_buf[row][x + 5] = 0x00;               /* inter-character gap */
    }
    s_dirty |= (uint8_t)(1u << row);
}

/** Push one page onto the wire: 1 control byte + 128 data bytes. */
static void send_page(uint8_t page)
{
    cmd((uint8_t)(0xB0 | page));    /* select the page   */
    cmd(0x00);                      /* column low nibble  */
    cmd(0x10);                      /* column high nibble */

    uint8_t pkt[1 + SSD1306_WIDTH];
    pkt[0] = CTRL_DATA;
    memcpy(&pkt[1], s_buf[page], SSD1306_WIDTH);
    i2c_write_blocking(s_i2c, s_addr, pkt, sizeof pkt, false);
}

int ssd1306_flush(void)
{
    int sent = 0;
    for (uint8_t page = 0; page < SSD1306_ROWS; page++) {
        if (s_dirty & (1u << page)) {
            send_page(page);
            sent++;
        }
    }
    s_dirty = 0;
    return sent;
}

int ssd1306_flush_all(void)
{
    s_dirty = 0xFF;
    return ssd1306_flush();
}
