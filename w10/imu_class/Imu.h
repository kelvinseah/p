/**
 * @file    Imu.h
 * @brief   C++ wrapper around the C LSM6DS3 (6-axis IMU) driver.
 *
 * This class does not add features. It adds a @e boundary: the I2C instance,
 * the "have I been initialised?" flag and the unit conversion all live inside
 * one object instead of being scattered through @c main().
 *
 * Compare with the Week 9 version, where every call site had to remember to
 * pass @c BOARD_I2C and to check that @c lsm6ds3_init() had already
 * succeeded. Forgetting either was possible. Here it is not.
 */
#ifndef ENG1104_IMU_H
#define ENG1104_IMU_H

#include "hardware/i2c.h"
#include "lsm6ds3.h"

/** One six-axis sample, in the units the rest of the system speaks. */
struct Sample {
    float ax = 0.0f;    /**< Acceleration along X, in g. */
    float ay = 0.0f;    /**< Acceleration along Y, in g. */
    float az = 0.0f;    /**< Acceleration along Z, in g. */
    float gx = 0.0f;    /**< Turning rate about X, in degrees per second. */
    float gy = 0.0f;    /**< Turning rate about Y, in dps. */
    float gz = 0.0f;    /**< Turning rate about Z, in dps. */
};

/**
 * @brief The 6-axis IMU, as an object.
 */
class Imu {
public:
    /**
     * @brief Bind the object to an I2C instance.
     *
     * Does not touch the hardware -- construction should not be able to fail.
     * Call ::begin for that.
     *
     * @param i2c  I2C instance the IMU is wired to.
     */
    explicit Imu(i2c_inst_t *i2c) : i2c_(i2c) {}

    /**
     * @brief Configure the IMU: 52 Hz output, +/-8 g, 2000 dps.
     *
     * The I2C peripheral must already be initialised by the caller.
     *
     * @return true if the IMU answered.
     */
    bool begin();

    /**
     * @brief Read one sample.
     *
     * @param out  Destination. Untouched on failure.
     * @return true on success; false if the bus failed or ::begin never
     *         succeeded.
     */
    bool read(Sample &out) const;

    /** @return true once ::begin has succeeded. */
    bool ready() const { return ready_; }

    /**
     * @brief Size of the acceleration, in g.
     *
     * About 1.0 at rest in any orientation, near 0 in free fall, and well
     * above 1 during an impact.
     */
    static float accelMagnitude(const Sample &s);

    /**
     * @brief Size of the turning rate, in degrees per second.
     *
     * A few dps at rest, tens while the pack is moved without turning, and
     * hundreds while the wearer's body rotates -- as it does in a fall.
     */
    static float gyroMagnitude(const Sample &s);

private:
    i2c_inst_t *i2c_;        /**< Not owned. */
    bool ready_ = false;     /**< Set by ::begin. */
};

#endif /* ENG1104_IMU_H */
