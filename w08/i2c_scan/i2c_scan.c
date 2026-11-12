/**
 * @file    i2c_scan.c
 * @brief   ENG1104 Week 8 Ex01 -- walk the I2C bus and report who answers.
 *
 * An I2C scan is the first thing to run whenever a sensor "does not work".
 * It separates two very different problems:
 *
 *   - nothing answers at all      -> wiring, power, or the wrong Grove port
 *   - something answers, but not
 *     at the address you expected -> wrong part, or a different module variant
 *
 * How it works: a 1-byte read is attempted at every legal address. A device
 * that is present pulls SDA low for the ACK bit and the SDK returns the number
 * of bytes read; an empty address leaves SDA high and the SDK returns a
 * negative error code. Nothing is written, so the scan cannot disturb a device.
 */
#include <stdio.h>

#include "board.h"
#include "hardware/i2c.h"
#include "pico/stdio_usb.h"
#include "pico/stdlib.h"

/**
 * @brief Decide whether an address is reserved by the I2C specification.
 *
 * 0x00-0x07 and 0x78-0x7F have special meanings (general call, 10-bit
 * addressing, and so on) and must never be probed.
 *
 * @param addr  7-bit address.
 * @return true if the address is reserved.
 */
static bool addr_reserved(uint8_t addr)
{
    /* TODO(student): Addresses 0x00-0x07 and 0x78-0x7F are reserved by the I2C
         specification and must never be probed. Return true for
         those. Hint: both ranges share the pattern (addr & 0x78).
     */
    return 0;
}

int main(void)
{
    stdio_init_all();
    /* Wait for the Serial Monitor so the first lines are not lost. */
    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }
    sleep_ms(200);

    i2c_init(BOARD_I2C, BOARD_I2C_BAUD);
    gpio_set_function(BOARD_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(BOARD_I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(BOARD_I2C_SDA);
    gpio_pull_up(BOARD_I2C_SCL);

    printf("\nENG1104 W08 Ex01 -- I2C scan\n");
    printf("SDA = GP%u, SCL = GP%u, %u kHz\n\n",
           (unsigned)BOARD_I2C_SDA, (unsigned)BOARD_I2C_SCL,
           (unsigned)(BOARD_I2C_BAUD / 1000u));

    printf("    0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");

    int found = 0;

    for (uint8_t addr = 0; addr < 0x80u; addr++) {
        if ((addr % 16u) == 0u) {
            printf("%02x ", addr);
        }

        bool present = false;
        if (!addr_reserved(addr)) {
            uint8_t rx;
            present = i2c_read_blocking(BOARD_I2C, addr, &rx, 1, false) >= 0;
        }

        printf(present ? " @ " : " . ");
        found += present ? 1 : 0;

        if ((addr % 16u) == 15u) {
            printf("\n");
        }
    }

    printf("\n%d device(s) found.\n", found);

    if (found == 0) {
        printf("Nothing on the bus. Check:\n");
        printf("  - the Grove cable is in Grove 3 (GP4/GP5)\n");
        printf("  - the GY-511 is the right way round\n");
        printf("  - the board's power switch is on\n");
    } else {
        printf("Expected: 0x19 = LSM303DLHC accelerometer\n");
        printf("          0x1e = LSM303DLHC magnetometer (unused here)\n");
    }

    while (true) {
        tight_loop_contents();
    }

    return 0;
}
