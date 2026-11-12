/**
 * @file    lsm6ds3.h
 * @brief   Driver for the Grove 6-axis IMU (ST LSM6DS3).
 * @author  ENG1104 teaching team
 *
 * The Grove 6-axis IMU carries an LSM6DS3 (the AY26/27 modules carry its
 * newer revision, the LSM6DS3TR-C): a 3-axis accelerometer and a 3-axis
 * gyroscope behind a single I2C address. It is a Grove module, so it plugs
 * straight into Grove 3 with the cable in its own bag.
 *
 * One IMU is issued per project group, and from Week 9 it is the sensor for
 * data collection and Lab Assessment 2. The GY-511 was the Week 8 learning
 * sensor; it is unplugged once the IMU arrives. The gyroscope is what the
 * GY-511 could not give you: it tells a fall, which rotates the body, apart
 * from sitting down, which barely does.
 *
 * @section lsm6ds3_vs_gy511 Three differences from the GY-511
 *
 * Both parts are ST, and the register style is the same -- one control
 * register per sub-sensor, then a burst read of the outputs. Three things do
 * differ, and each one is a place to get it wrong:
 *
 * -# @b Auto-increment @b is @b already @b on. The LSM303DLHC needs bit 7 of
 *    the sub-address set (::GY511_AUTO_INC) or a burst read returns the same
 *    register over and over. On the LSM6DS3 the IF_INC bit of CTRL3_C does
 *    that job and is set at power-on, so the sub-address is sent plain.
 * -# @b The @b data @b is @b right-justified. The LSM303DLHC returns a
 *    12-bit value left-justified in 16 bits, which is why ::gy511_to_g
 *    shifts right by 4. The LSM6DS3 returns a full 16-bit value -- shifting
 *    it would throw away four bits of real resolution.
 * -# @b There @b is @b a @b WHO_AM_I. ::LSM6DS3_WHO_AM_I reads back
 *    ::LSM6DS3_WHO_AM_I_VALUE, so ::lsm6ds3_init can identify the part
 *    properly instead of writing a register and reading it back.
 *
 * @section lsm6ds3_odr Why 52 Hz and not 50
 *
 * The sampling loop runs at a strict 50 Hz off a hardware timer. The LSM6DS3
 * has no 50 Hz setting -- its output data rates step 12.5, 26, 52, 104 Hz --
 * so it is configured for 52 Hz, the nearest rate at or above the rate we
 * read at. Configuring it slower (26 Hz) would mean re-reading stale samples
 * roughly half the time. The timer still decides when a sample is taken; the
 * sensor is only asked to have a fresh one ready.
 *
 * @section lsm6ds3_ranges Full-scale choices
 *
 * - @b Accelerometer @b +/-8 g, matching the GY-511. An impact easily
 *   exceeds +/-2 g, and a clipped impact makes every fall look the same.
 * - @b Gyroscope @b 2000 dps. A trunk rotating through a fall passes several
 *   hundred dps, and the cost of the wide range is only resolution, of which
 *   a 16-bit output has plenty.
 *
 * @section lsm6ds3_burst One burst, both sensors
 *
 * The gyroscope outputs at 0x22-0x27 are immediately followed by the
 * accelerometer outputs at 0x28-0x2D. With auto-increment on, ::lsm6ds3_read_raw
 * fetches all twelve bytes in a single transaction -- about 250 us at
 * 400 kHz -- so both sub-sensors come from the same instant and the ISR pays
 * for one bus transaction, not two.
 */
#ifndef ENG1104_LSM6DS3_H
#define ENG1104_LSM6DS3_H

#include <stdbool.h>
#include <stdint.h>

#include "hardware/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 7-bit I2C address, with SA0 pulled low on the Grove board.
 *
 * @note If an I2C scan shows 0x6B instead, the module ties SA0 high. Change
 *       this one value; nothing else in the driver depends on it.
 */
#define LSM6DS3_ADDR            0x6Au

/** @name Register map @{ */
#define LSM6DS3_WHO_AM_I        0x0Fu   /**< Fixed identity register. */
#define LSM6DS3_CTRL1_XL        0x10u   /**< Accelerometer ODR + full scale. */
#define LSM6DS3_CTRL2_G         0x11u   /**< Gyroscope ODR + full scale. */
#define LSM6DS3_CTRL3_C         0x12u   /**< BDU and register auto-increment. */
#define LSM6DS3_OUTX_L_G        0x22u   /**< First of six gyroscope bytes. */
#define LSM6DS3_OUTX_L_XL       0x28u   /**< First of six accelerometer bytes. */
/** @} */

/** Value ::LSM6DS3_WHO_AM_I returns on the original LSM6DS3. */
#define LSM6DS3_WHO_AM_I_VALUE  0x69u

/**
 * Value ::LSM6DS3_WHO_AM_I returns on the LSM6DS3TR-C, the revision fitted
 * to the Grove modules issued in AY26/27 (measured 9 Oct 2026). Every
 * register and setting this driver uses is identical on both parts.
 */
#define LSM6DS3TRC_WHO_AM_I_VALUE  0x6Au

/** CTRL1_XL: 52 Hz output data rate, +/-8 g full scale. */
#define LSM6DS3_XL_52HZ_8G      0x3Cu

/** CTRL2_G: 52 Hz output data rate, 2000 dps full scale. */
#define LSM6DS3_G_52HZ_2000DPS  0x3Cu

/** CTRL3_C: block data update on, register auto-increment on. */
#define LSM6DS3_BDU_IF_INC      0x44u

/** Accelerometer sensitivity at +/-8 g, in g per count. */
#define LSM6DS3_SCALE_8G        0.000244f

/** Gyroscope sensitivity at 2000 dps, in degrees per second per count. */
#define LSM6DS3_SCALE_2000DPS   0.070f

/** One six-axis sample: raw counts, exactly as they left the sensor. */
typedef struct {
    int16_t gx;  /**< Raw angular rate about X. */
    int16_t gy;  /**< Raw angular rate about Y. */
    int16_t gz;  /**< Raw angular rate about Z. */
    int16_t ax;  /**< Raw acceleration along X. */
    int16_t ay;  /**< Raw acceleration along Y. */
    int16_t az;  /**< Raw acceleration along Z. */
} lsm6ds3_raw_t;

/** One six-axis sample in engineering units. */
typedef struct {
    float gx;    /**< Angular rate about X, in degrees per second. */
    float gy;    /**< Angular rate about Y, in dps. */
    float gz;    /**< Angular rate about Z, in dps. */
    float ax;    /**< Acceleration along X, in g. */
    float ay;    /**< Acceleration along Y, in g. */
    float az;    /**< Acceleration along Z, in g. */
} lsm6ds3_sample_t;

/**
 * @brief Identify the device, then configure both sub-sensors.
 *
 * Reads WHO_AM_I first and gives up unless it is ::LSM6DS3_WHO_AM_I_VALUE or
 * ::LSM6DS3TRC_WHO_AM_I_VALUE,
 * so a mis-wired bus fails here rather than a hundred samples later. Then
 * writes CTRL3_C (block data update, auto-increment), CTRL1_XL (52 Hz,
 * +/-8 g) and CTRL2_G (52 Hz, 2000 dps).
 *
 * The I2C peripheral itself must already be initialised by the caller --
 * @c i2c_init(), @c gpio_set_function() on SDA and SCL, and pull-ups. When
 * the GY-511 is on the same bus, initialise it separately; the two devices
 * have different addresses and do not interfere.
 *
 * @param i2c   I2C instance the module is wired to, e.g. @c i2c0.
 * @return true if an LSM6DS3 or LSM6DS3TR-C answered and every write was
 *         accepted.
 */
bool lsm6ds3_init(i2c_inst_t *i2c);

/**
 * @brief Read gyroscope and accelerometer in one twelve-byte burst.
 *
 * @param i2c   I2C instance.
 * @param out   Destination for the raw counts. Not modified on failure.
 * @return true on success, false if the bus transaction failed.
 */
bool lsm6ds3_read_raw(i2c_inst_t *i2c, lsm6ds3_raw_t *out);

/**
 * @brief Read all six axes and convert them to g and dps.
 *
 * @param i2c   I2C instance.
 * @param out   Destination for the converted sample.
 * @return true on success, false if the bus transaction failed.
 */
bool lsm6ds3_read(i2c_inst_t *i2c, lsm6ds3_sample_t *out);

/**
 * @brief Convert one raw accelerometer count to g.
 *
 * @param raw   Raw 16-bit word from the sensor.
 * @return Acceleration in g.
 *
 * @note No shift, unlike ::gy511_to_g. This part right-justifies its output.
 */
static inline float lsm6ds3_to_g(int16_t raw)
{
    return (float)raw * LSM6DS3_SCALE_8G;
}

/**
 * @brief Convert one raw gyroscope count to degrees per second.
 *
 * @param raw   Raw 16-bit word from the sensor.
 * @return Angular rate in dps.
 */
static inline float lsm6ds3_to_dps(int16_t raw)
{
    return (float)raw * LSM6DS3_SCALE_2000DPS;
}

/**
 * @brief Magnitude of the acceleration vector, in g.
 *
 * At rest this reads about 1.0 whatever the orientation, which makes it a
 * convenient single number for impact detection -- the same role
 * ::gy511_magnitude plays for the GY-511.
 *
 * @param s  Converted sample.
 * @return sqrt(ax^2 + ay^2 + az^2).
 */
float lsm6ds3_accel_magnitude(const lsm6ds3_sample_t *s);

/**
 * @brief Magnitude of the angular rate vector, in degrees per second.
 *
 * A few dps at rest, tens of dps while the pack is pushed along without
 * turning, and hundreds to thousands while it rotates (measured 9 Oct 2026). This is the channel the accelerometer cannot
 * give you, and the reason the IMU is in the kit at all.
 *
 * @param s  Converted sample.
 * @return sqrt(gx^2 + gy^2 + gz^2).
 */
float lsm6ds3_gyro_magnitude(const lsm6ds3_sample_t *s);

#ifdef __cplusplus
}
#endif

#endif /* ENG1104_LSM6DS3_H */
