/**
 * @file    Imu.cpp
 * @brief   Implementation of the Imu class.
 */
#include "Imu.h"

#include <cmath>

bool Imu::begin()
{
    ready_ = lsm6ds3_init(i2c_);
    return ready_;
}

bool Imu::read(Sample &out) const
{
    if (!ready_) {
        return false;
    }

    lsm6ds3_sample_t m;
    if (!lsm6ds3_read(i2c_, &m)) {
        return false;
    }

    out.ax = m.ax;
    out.ay = m.ay;
    out.az = m.az;
    out.gx = m.gx;
    out.gy = m.gy;
    out.gz = m.gz;
    return true;
}

float Imu::accelMagnitude(const Sample &s)
{
    return std::sqrt(s.ax * s.ax + s.ay * s.ay + s.az * s.az);
}

float Imu::gyroMagnitude(const Sample &s)
{
    return std::sqrt(s.gx * s.gx + s.gy * s.gy + s.gz * s.gz);
}
