/**
 * @file    gy511.c
 * @brief   Implementation of the GY-511 (LSM303DLHC) accelerometer driver.
 *
 * See gy511.h for the reasoning behind the register values.
 */
#include "gy511.h"

#include <math.h>

/**
 * @brief Write one byte to one register.
 *
 * @param i2c   I2C instance.
 * @param reg   Register sub-address.
 * @param val   Value to write.
 * @return true if both bytes were accepted.
 */
static bool write_reg(i2c_inst_t *i2c, uint8_t reg, uint8_t val)
{
    const uint8_t buf[2] = { reg, val };
    return i2c_write_blocking(i2c, GY511_ADDR, buf, 2, false) == 2;
}

/**
 * @brief Read a block of consecutive registers.
 *
 * The sub-address is sent with ::GY511_AUTO_INC set, without which the sensor
 * would return the same register @p len times. The write is ended with a
 * repeated start (@c nostop = true) so the bus is not released between the
 * address phase and the data phase.
 *
 * @param i2c   I2C instance.
 * @param reg   First register sub-address.
 * @param dst   Destination buffer.
 * @param len   Number of bytes to read.
 * @return true if the whole block was read.
 */
static bool read_regs(i2c_inst_t *i2c, uint8_t reg, uint8_t *dst, size_t len)
{
    const uint8_t sub = reg | GY511_AUTO_INC;

    if (i2c_write_blocking(i2c, GY511_ADDR, &sub, 1, true) != 1) {
        return false;
    }
    return i2c_read_blocking(i2c, GY511_ADDR, dst, len, false) == (int)len;
}

bool gy511_init(i2c_inst_t *i2c)
{
    if (!write_reg(i2c, GY511_CTRL_REG1_A, GY511_ODR_50HZ_XYZ)) {
        return false;
    }
    if (!write_reg(i2c, GY511_CTRL_REG4_A, GY511_FS_8G_HR_BDU)) {
        return false;
    }

    /* No WHO_AM_I on this part, so confirm the write actually landed. */
    uint8_t check = 0;
    if (!read_regs(i2c, GY511_CTRL_REG1_A, &check, 1)) {
        return false;
    }
    return check == GY511_ODR_50HZ_XYZ;
}

bool gy511_read_raw(i2c_inst_t *i2c, gy511_raw_t *out)
{
    uint8_t buf[6];

    if (!read_regs(i2c, GY511_OUT_X_L_A, buf, sizeof(buf))) {
        return false;
    }

    /* Little-endian by default (CTRL_REG4_A BLE = 0): low byte first. */
    out->x = (int16_t)((uint16_t)buf[1] << 8 | buf[0]);
    out->y = (int16_t)((uint16_t)buf[3] << 8 | buf[2]);
    out->z = (int16_t)((uint16_t)buf[5] << 8 | buf[4]);
    return true;
}

bool gy511_read_g(i2c_inst_t *i2c, gy511_g_t *out)
{
    gy511_raw_t raw;

    if (!gy511_read_raw(i2c, &raw)) {
        return false;
    }

    out->x = gy511_to_g(raw.x);
    out->y = gy511_to_g(raw.y);
    out->z = gy511_to_g(raw.z);
    return true;
}

float gy511_magnitude(const gy511_g_t *s)
{
    return sqrtf(s->x * s->x + s->y * s->y + s->z * s->z);
}
