/**
 * @file    gy511.h
 * @brief   Driver for the GY-511 (ST LSM303DLHC) 3-axis accelerometer.
 * @author  ENG1104 teaching team
 *
 * The GY-511 carries an LSM303DLHC, which contains two independent I2C
 * devices: a linear accelerometer at 0x19 and a magnetometer at 0x1E. Only
 * the accelerometer is used in ENG1104.
 *
 * @section gy511_config Why the configuration matters
 *
 * Two register writes decide whether the sensor can see a fall at all.
 *
 * - @b CTRL_REG1_A sets the output data rate. Lab Assessment 2 samples at a
 *   strict 50 Hz, so the sensor is configured for 50 Hz too. Running the
 *   sensor faster than you read it wastes power and gains nothing; running it
 *   slower means you re-read stale samples.
 * - @b CTRL_REG4_A sets the full-scale range. The power-on default is
 *   @b +/-2 g, and an impact easily exceeds that -- the reading simply clips,
 *   and every fall then looks identical to every other fall. ::gy511_init
 *   selects +/-8 g so impacts stay on-scale.
 *
 * @section gy511_autoinc Reading more than one register
 *
 * The LSM303DLHC only auto-increments its register pointer when bit 7 of the
 * sub-address is set. Without it, a six-byte burst read returns the same
 * register six times. ::GY511_AUTO_INC is that bit, and ::gy511_read_raw
 * relies on it to fetch X, Y and Z in a single transaction -- which, together
 * with the BDU (block data update) bit, guarantees the three axes come from
 * the same sample.
 *
 * @section gy511_scaling Converting counts to g
 *
 * In high-resolution mode the sensor returns a 12-bit value left-justified
 * inside a 16-bit word, so the raw word must be shifted right by 4 before
 * scaling. At +/-8 g the sensitivity is 4 mg per count, so 1 g is 250 counts.
 */
#ifndef ENG1104_GY511_H
#define ENG1104_GY511_H

#include <stdbool.h>
#include <stdint.h>

#include "hardware/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 7-bit I2C address of the LSM303DLHC linear accelerometer. */
#define GY511_ADDR          0x19u

/** @name Register map (accelerometer block) @{ */
#define GY511_CTRL_REG1_A   0x20u   /**< ODR + axis enables. */
#define GY511_CTRL_REG4_A   0x23u   /**< BDU, endianness, full scale, HR. */
#define GY511_OUT_X_L_A     0x28u   /**< First of six output registers. */
/** @} */

/** OR this into a sub-address to enable register auto-increment. */
#define GY511_AUTO_INC      0x80u

/** CTRL_REG1_A: 50 Hz output data rate, X/Y/Z enabled, normal power mode. */
#define GY511_ODR_50HZ_XYZ  0x47u

/** CTRL_REG4_A: block data update, little-endian, +/-8 g, high resolution. */
#define GY511_FS_8G_HR_BDU  0xA8u

/** Sensitivity at +/-8 g in high-resolution mode, in g per count. */
#define GY511_SCALE_8G_HR   0.004f

/** One sample: raw counts, exactly as they left the sensor. */
typedef struct {
    int16_t x;  /**< Raw X, 12-bit value left-justified in 16 bits. */
    int16_t y;  /**< Raw Y. */
    int16_t z;  /**< Raw Z. */
} gy511_raw_t;

/** One sample converted to multiples of standard gravity. */
typedef struct {
    float x;    /**< Acceleration along X, in g. */
    float y;    /**< Acceleration along Y, in g. */
    float z;    /**< Acceleration along Z, in g. */
} gy511_g_t;

/**
 * @brief Configure the accelerometer for 50 Hz sampling at +/-8 g.
 *
 * Writes CTRL_REG1_A and CTRL_REG4_A, then reads CTRL_REG1_A back to confirm
 * a real device answered. The LSM303DLHC accelerometer has no WHO_AM_I
 * register, so a read-back is the available presence check.
 *
 * The I2C peripheral itself must already be initialised by the caller --
 * @c i2c_init(), @c gpio_set_function() on SDA and SCL, and pull-ups.
 *
 * @param i2c   I2C instance the sensor is wired to, e.g. @c i2c0.
 * @return true if the sensor acknowledged and read back the expected value.
 */
bool gy511_init(i2c_inst_t *i2c);

/**
 * @brief Read all three axes in one auto-incremented burst.
 *
 * @param i2c   I2C instance.
 * @param out   Destination for the raw counts. Not modified on failure.
 * @return true on success, false if the bus transaction failed.
 */
bool gy511_read_raw(i2c_inst_t *i2c, gy511_raw_t *out);

/**
 * @brief Read all three axes and convert them to g.
 *
 * @param i2c   I2C instance.
 * @param out   Destination for the converted sample.
 * @return true on success, false if the bus transaction failed.
 */
bool gy511_read_g(i2c_inst_t *i2c, gy511_g_t *out);

/**
 * @brief Convert one raw axis count to g at the +/-8 g high-resolution setting.
 *
 * @param raw   Raw 16-bit word from the sensor.
 * @return Acceleration in g.
 */
static inline float gy511_to_g(int16_t raw)
{
    /* Arithmetic shift discards the four unused low bits and keeps the sign. */
    return (float)(raw >> 4) * GY511_SCALE_8G_HR;
}

/**
 * @brief Magnitude of a sample, in g.
 *
 * At rest this reads about 1.0 whatever the orientation, which makes it a
 * convenient single number for impact detection.
 *
 * @param s  Converted sample.
 * @return sqrt(x^2 + y^2 + z^2).
 */
float gy511_magnitude(const gy511_g_t *s);

#ifdef __cplusplus
}
#endif

#endif /* ENG1104_GY511_H */
