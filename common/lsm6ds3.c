/**
 * @file    lsm6ds3.c
 * @brief   Implementation of the Grove 6-axis IMU (LSM6DS3) driver.
 *
 * See lsm6ds3.h for the reasoning behind the register values, and for the
 * three ways this part differs from the GY-511.
 */
#include "lsm6ds3.h"

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
    return i2c_write_blocking(i2c, LSM6DS3_ADDR, buf, 2, false) == 2;
}

/**
 * @brief Read a block of consecutive registers.
 *
 * The sub-address is sent plain -- no auto-increment bit to OR in, because
 * IF_INC in CTRL3_C already does that job. The write is ended with a repeated
 * start (@c nostop = true) so the bus is not released between the address
 * phase and the data phase.
 *
 * @param i2c   I2C instance.
 * @param reg   First register sub-address.
 * @param dst   Destination buffer.
 * @param len   Number of bytes to read.
 * @return true if the whole block was read.
 */
static bool read_regs(i2c_inst_t *i2c, uint8_t reg, uint8_t *dst, size_t len)
{
    if (i2c_write_blocking(i2c, LSM6DS3_ADDR, &reg, 1, true) != 1) {
        return false;
    }
    return i2c_read_blocking(i2c, LSM6DS3_ADDR, dst, len, false) == (int)len;
}

bool lsm6ds3_init(i2c_inst_t *i2c)
{
    uint8_t who = 0;

    /* Identify the part before configuring it. A mis-wired bus, a module at
     * 0x6B, or a different chip entirely all fail here, which is a much
     * easier fault to read than samples that are quietly always zero. */
    if (!read_regs(i2c, LSM6DS3_WHO_AM_I, &who, 1)) {
        return false;
    }
    if (who != LSM6DS3_WHO_AM_I_VALUE && who != LSM6DS3TRC_WHO_AM_I_VALUE) {
        return false;
    }

    /* BDU first: it stops a burst read from mixing the high byte of one
     * sample with the low byte of the next. IF_INC is set in the same write
     * because read_regs() depends on it. */
    if (!write_reg(i2c, LSM6DS3_CTRL3_C, LSM6DS3_BDU_IF_INC)) {
        return false;
    }
    if (!write_reg(i2c, LSM6DS3_CTRL1_XL, LSM6DS3_XL_52HZ_8G)) {
        return false;
    }
    if (!write_reg(i2c, LSM6DS3_CTRL2_G, LSM6DS3_G_52HZ_2000DPS)) {
        return false;
    }

    /* The output registers hold zero until the first conversion finishes,
     * and the gyroscope needs tens of milliseconds to start up. Without this
     * wait the first sample read after init is all zeros (seen on hardware,
     * 9 Oct 2026). */
    sleep_ms(100);
    return true;
}

bool lsm6ds3_read_raw(i2c_inst_t *i2c, lsm6ds3_raw_t *out)
{
    uint8_t buf[12];

    /* Gyroscope 0x22-0x27 runs straight into accelerometer 0x28-0x2D, so one
     * burst from OUTX_L_G collects both sub-sensors from the same instant. */
    if (!read_regs(i2c, LSM6DS3_OUTX_L_G, buf, sizeof(buf))) {
        return false;
    }

    /* Little-endian: low byte first, and the value is already right-
     * justified across all 16 bits. */
    out->gx = (int16_t)((uint16_t)buf[1]  << 8 | buf[0]);
    out->gy = (int16_t)((uint16_t)buf[3]  << 8 | buf[2]);
    out->gz = (int16_t)((uint16_t)buf[5]  << 8 | buf[4]);
    out->ax = (int16_t)((uint16_t)buf[7]  << 8 | buf[6]);
    out->ay = (int16_t)((uint16_t)buf[9]  << 8 | buf[8]);
    out->az = (int16_t)((uint16_t)buf[11] << 8 | buf[10]);
    return true;
}

bool lsm6ds3_read(i2c_inst_t *i2c, lsm6ds3_sample_t *out)
{
    lsm6ds3_raw_t raw;

    if (!lsm6ds3_read_raw(i2c, &raw)) {
        return false;
    }

    out->gx = lsm6ds3_to_dps(raw.gx);
    out->gy = lsm6ds3_to_dps(raw.gy);
    out->gz = lsm6ds3_to_dps(raw.gz);
    out->ax = lsm6ds3_to_g(raw.ax);
    out->ay = lsm6ds3_to_g(raw.ay);
    out->az = lsm6ds3_to_g(raw.az);
    return true;
}

float lsm6ds3_accel_magnitude(const lsm6ds3_sample_t *s)
{
    return sqrtf(s->ax * s->ax + s->ay * s->ay + s->az * s->az);
}

float lsm6ds3_gyro_magnitude(const lsm6ds3_sample_t *s)
{
    return sqrtf(s->gx * s->gx + s->gy * s->gy + s->gz * s->gz);
}
