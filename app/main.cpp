/**
 * @file    main.cpp
 * @brief   projectile -- servo launcher + reed-switch stop + IMU logging.
 *
 * Everything specific to this project lives in this one file. The course
 * files (board.h, common/, w08..w10) are untouched; from them only the servo
 * helpers in actuators.h are used.
 *
 * ---------------------------------------------------------------------------
 * Wiring (bare Pico WH on perfboard)
 * ---------------------------------------------------------------------------
 *   Power        VSYS (pin 39), GND (pin 38)
 *   Servo        VSYS (pin 39), GND (pin 33), SIG pin 34 = GP28
 *   IMU          3V3 OUT (pin 36), GND (pin 28),
 *                SDA pin 29 = GP22, SCL pin 27 = GP21
 *   Reed switch  3V3 OUT (pin 36), GND (pin 23), SIG pin 21 = GP16
 *
 * ---------------------------------------------------------------------------
 * What it does
 * ---------------------------------------------------------------------------
 *   1. Starts at 0 degrees, waits 1 s.
 *   2. For each angle in kShotAngles (90, then 0): prints a LAUNCH marker,
 *      moves the servo there ("shot"), and logs the IMU as CSV for kLogMs.
 *   3. Holds at 0 degrees.
 *   A magnet on the reed switch (held ~30 ms) stops everything at any point
 *   and sends the servo back to 0.
 *
 * ---------------------------------------------------------------------------
 * Using it with Tracker
 * ---------------------------------------------------------------------------
 *   Open the Serial Monitor before the first shot. Lines starting with '#'
 *   are messages; every other line is CSV:
 *
 *       t_ms,shot,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps
 *
 *   t_ms is time since that shot's LAUNCH marker, so t_ms = 0 is the frame
 *   in the video where the servo arm starts to move. Copy the CSV lines into
 *   Excel next to Tracker's export to line the two up.
 *
 * ---------------------------------------------------------------------------
 * Why the IMU code is in here
 * ---------------------------------------------------------------------------
 *   The RP2040's hardware I2C needs SDA and SCL from the SAME controller.
 *   GP22 is i2c1-SDA and GP21 is i2c0-SCL, so the hardware cannot use this
 *   pair, and the course lsm6ds3 driver (which needs a hardware i2c_inst_t)
 *   cannot either. This file drives the bus in software on those two pins
 *   instead, and talks to the LSM6DS3 directly.
 */
#include <cstdint>
#include <cstdio>

#include "actuators.h"          /* servo_init(), servo_write_deg(), tone_*() */
#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "pico/time.h"

/* ======================================================================== */
/*  Settings -- change these                                                */
/* ======================================================================== */

static constexpr uint kServoPin = 28;   /* pin 34 */
static constexpr uint kReedPin  = 16;   /* pin 21, 0 = magnet present */
static constexpr uint kImuSda   = 22;   /* pin 29 */
static constexpr uint kImuScl   = 21;   /* pin 27 */

/** Buzzer GPIO, or -1 for none. Never 21 or 22: those are the IMU bus.
 *  A piezo on GP15 (pin 20) works: set this to 15. */
static constexpr int kBuzzerPin = -1;

/** Servo angles to move to, one "shot" each, in order. */
static constexpr float kShotAngles[] = { 90.0f, 0.0f };

static constexpr float kZeroOffset = 0.0f;   /* adjust until 0 looks right */
static constexpr float kRestAngle  = 0.0f;   /* where it starts and ends */

static constexpr uint32_t kStartDelayMs = 1000;  /* before the first shot */
static constexpr uint32_t kLogMs        = 1000;  /* IMU logging per shot */
static constexpr uint32_t kLogPeriodMs  = 10;    /* 100 samples per second */
static constexpr uint32_t kShotSoundMs  = 150;   /* gunshot length */

/* ======================================================================== */
/*  Software I2C on kImuSda / kImuScl                                       */
/* ======================================================================== */
/*
 * Open-drain emulation: a line is pulled LOW by making the pin an output
 * driving 0, and RELEASED by making it an input so the pull-ups take it high.
 * Nothing ever drives a line high. About 250 kHz; a 12-byte IMU read takes
 * roughly 0.6 ms.
 */
namespace sw_i2c {

static constexpr uint32_t kHalfUs = 2;          /* half clock period */
static constexpr uint32_t kStretchTimeoutUs = 1000;

static bool fault = false;

static inline void half() { busy_wait_us_32(kHalfUs); }
static inline void sdaLow()     { gpio_set_dir(kImuSda, GPIO_OUT); }
static inline void sdaRelease() { gpio_set_dir(kImuSda, GPIO_IN); }
static inline void sclLow()     { gpio_set_dir(kImuScl, GPIO_OUT); }

static inline void sclRelease()
{
    gpio_set_dir(kImuScl, GPIO_IN);
    uint32_t waited = 0;
    while (!gpio_get(kImuScl)) {               /* clock stretching */
        if (++waited > kStretchTimeoutUs) {
            fault = true;
            return;
        }
        busy_wait_us_32(1);
    }
}

/** START, or repeated START when the bus is still held. */
static void start()
{
    sdaRelease(); half();
    sclRelease(); half();
    sdaLow();     half();
    sclLow();     half();
}

static void stop()
{
    sdaLow();     half();
    sclRelease(); half();
    sdaRelease(); half();
}

/** @return true if the device acknowledged. */
static bool writeByte(uint8_t b)
{
    for (int i = 7; i >= 0; --i) {
        if (b & (1u << i)) {
            sdaRelease();
        } else {
            sdaLow();
        }
        half();
        sclRelease(); half();
        sclLow();
    }
    sdaRelease(); half();
    sclRelease(); half();
    const bool ack = !gpio_get(kImuSda);
    sclLow();
    return ack;
}

static uint8_t readByte(bool ack)
{
    uint8_t b = 0;
    sdaRelease();
    for (int i = 0; i < 8; ++i) {
        half();
        sclRelease(); half();
        b = static_cast<uint8_t>((b << 1) | (gpio_get(kImuSda) ? 1u : 0u));
        sclLow();
    }
    if (ack) {
        sdaLow();
    } else {
        sdaRelease();
    }
    half();
    sclRelease(); half();
    sclLow();
    sdaRelease();
    return b;
}

static void initPin(uint pin)
{
    gpio_init(pin);
    gpio_put(pin, 0);             /* output level is always 0 ...           */
    gpio_set_dir(pin, GPIO_IN);   /* ... and the pin starts released (high) */
    gpio_pull_up(pin);
}

static void init()
{
    initPin(kImuSda);
    initPin(kImuScl);
    half();
    /* Bus recovery: clock out a device left mid-byte by a reset. */
    for (int i = 0; i < 9 && !gpio_get(kImuSda); ++i) {
        sclLow();     half();
        sclRelease(); half();
    }
    stop();
}

/** Write one register. */
static bool writeReg(uint8_t addr, uint8_t reg, uint8_t val)
{
    fault = false;
    start();
    const bool ok = writeByte(static_cast<uint8_t>(addr << 1)) &&
                    writeByte(reg) && writeByte(val);
    stop();
    return ok && !fault;
}

/** Read len consecutive registers starting at reg. */
static bool readRegs(uint8_t addr, uint8_t reg, uint8_t *dst, std::size_t len)
{
    fault = false;
    start();
    if (!writeByte(static_cast<uint8_t>(addr << 1)) || !writeByte(reg)) {
        stop();
        return false;
    }
    start();                                     /* repeated START */
    if (!writeByte(static_cast<uint8_t>((addr << 1) | 1u))) {
        stop();
        return false;
    }
    for (std::size_t i = 0; i < len; ++i) {
        dst[i] = readByte(i + 1 < len);
    }
    stop();
    return !fault;
}

}  // namespace sw_i2c

/* ======================================================================== */
/*  LSM6DS3 IMU                                                             */
/* ======================================================================== */
namespace imu {

static constexpr uint8_t kWhoAmI   = 0x0F;
static constexpr uint8_t kCtrl1Xl  = 0x10;
static constexpr uint8_t kCtrl2G   = 0x11;
static constexpr uint8_t kCtrl3C   = 0x12;
static constexpr uint8_t kOutxLG   = 0x22;   /* gyro 0x22..0x27, accel 0x28..0x2D */

/* 208 Hz output for both sensors -- a servo throw is fast, so this is
 * quicker than the course's 52 Hz. +/-8 g and 2000 dps as in the course. */
static constexpr uint8_t kXl208Hz8g    = 0x5C;
static constexpr uint8_t kG208Hz2000   = 0x5C;
static constexpr uint8_t kBduIfInc     = 0x44;

static constexpr float kGPerLsb   = 0.000244f;   /* +/-8 g */
static constexpr float kDpsPerLsb = 0.070f;      /* 2000 dps */

static uint8_t addr = 0x6A;

struct Sample {
    float ax, ay, az;   /* g */
    float gx, gy, gz;   /* degrees per second */
};

/** Find the IMU at 0x6A or 0x6B and configure it. */
static bool begin()
{
    const uint8_t candidates[] = { 0x6A, 0x6B };
    for (uint8_t a : candidates) {
        uint8_t who = 0;
        /* 0x69 = LSM6DS3, 0x6A = LSM6DS3TR-C */
        if (sw_i2c::readRegs(a, kWhoAmI, &who, 1) &&
            (who == 0x69 || who == 0x6A)) {
            addr = a;
            if (!sw_i2c::writeReg(addr, kCtrl3C, kBduIfInc) ||
                !sw_i2c::writeReg(addr, kCtrl1Xl, kXl208Hz8g) ||
                !sw_i2c::writeReg(addr, kCtrl2G, kG208Hz2000)) {
                return false;
            }
            sleep_ms(100);   /* first conversions are zero otherwise */
            return true;
        }
    }
    return false;
}

static inline int16_t le16(const uint8_t *p)
{
    return static_cast<int16_t>(static_cast<uint16_t>(p[1]) << 8 | p[0]);
}

static bool read(Sample &s)
{
    uint8_t b[12];
    if (!sw_i2c::readRegs(addr, kOutxLG, b, sizeof b)) {
        return false;
    }
    s.gx = le16(b + 0)  * kDpsPerLsb;
    s.gy = le16(b + 2)  * kDpsPerLsb;
    s.gz = le16(b + 4)  * kDpsPerLsb;
    s.ax = le16(b + 6)  * kGPerLsb;
    s.ay = le16(b + 8)  * kGPerLsb;
    s.az = le16(b + 10) * kGPerLsb;
    return true;
}

}  // namespace imu

/* ======================================================================== */
/*  Reed switch                                                             */
/* ======================================================================== */

static bool g_stopped = false;
static int g_closed_count = 0;

static void reedInit(void)
{
    gpio_init(kReedPin);
    gpio_set_dir(kReedPin, GPIO_IN);
    gpio_pull_up(kReedPin);             /* enable internal pull-up */
    printf("[REED] Ready on GP%d (open=%d)\n", kReedPin, gpio_get(kReedPin));
}

/** Call about every 10 ms. Stops after 3 closed readings in a row (~30 ms). */
static void reedPoll(void)
{
    if (!gpio_get(kReedPin)) {          /* 0 = closed (magnet) */
        g_closed_count++;

        if (g_closed_count == 1) {
            printf("[REED] Magnet detected...\n");
        }
        if (g_closed_count == 3) {      /* == so it only prints once */
            g_stopped = true;
            printf("[REED] Magnet CONFIRMED -> STOP\n");
        }
    } else {
        if (g_closed_count > 0 && !g_stopped) {
            printf("[REED] Magnet lost (only %d readings, ignored)\n",
                   g_closed_count);
        }
        g_closed_count = 0;
    }
}

/** Wait ms milliseconds, checking the reed switch every 10 ms. */
static void waitMs(uint32_t ms)
{
    for (uint32_t t = 0; t < ms && !g_stopped; t += 10) {
        reedPoll();
        sleep_ms(10);
    }
}

/* ======================================================================== */
/*  Gunshot sound (only if kBuzzerPin is set)                               */
/* ======================================================================== */

static uint32_t g_rng = 12345;

static int randomRange(int lo, int hi)
{
    g_rng = g_rng * 1103515245u + 12345u;
    return lo + static_cast<int>((g_rng >> 16) % static_cast<uint32_t>(hi - lo + 1));
}

/** One step of the gunshot at t ms into it: noisy crack, falling pitch, fade. */
static void shotSoundStep(uint32_t t)
{
    if (kBuzzerPin < 0) {
        return;
    }
    const uint pin = static_cast<uint>(kBuzzerPin);
    if (t >= kShotSoundMs) {
        tone_stop(pin);
        return;
    }
    const float k = static_cast<float>(t) / static_cast<float>(kShotSoundMs);
    const float base = 2500.0f - 2300.0f * k;
    const int noise = static_cast<int>(1200.0f * (1.0f - k)) + 50;
    float freq = base + static_cast<float>(randomRange(-noise, noise));
    if (freq < 120.0f) {
        freq = 120.0f;
    }
    float vol = 0.5f * (1.0f - k);
    if (vol < 0.01f) {
        vol = 0.01f;
    }
    tone_play(pin, freq, vol);
}

/* ======================================================================== */
/*  One shot                                                                */
/* ======================================================================== */

static void moveServo(float angle)
{
    servo_write_deg(kServoPin, angle + kZeroOffset);
}

/**
 * Move to @p angle and log the IMU for kLogMs. Runs on a 1 ms tick: the
 * sound is updated every tick, the IMU and the reed switch every
 * kLogPeriodMs.
 */
static void shot(int n, float angle, bool have_imu)
{
    const absolute_time_t t0 = get_absolute_time();
    std::printf("# LAUNCH %d angle=%.1f boot_ms=%lu\n", n, angle,
                static_cast<unsigned long>(to_ms_since_boot(t0)));

    moveServo(angle);

    absolute_time_t next = t0;
    for (uint32_t t = 0; t <= kLogMs && !g_stopped; ++t) {
        if (t <= kShotSoundMs) {
            shotSoundStep(t);
        }
        if (t % kLogPeriodMs == 0) {
            reedPoll();
            imu::Sample s;
            if (have_imu && imu::read(s)) {
                std::printf("%lu,%d,%.3f,%.3f,%.3f,%.1f,%.1f,%.1f\n",
                            static_cast<unsigned long>(t), n,
                            s.ax, s.ay, s.az, s.gx, s.gy, s.gz);
            }
        }
        next = delayed_by_ms(next, 1);
        sleep_until(next);
    }
    if (kBuzzerPin >= 0) {
        tone_stop(static_cast<uint>(kBuzzerPin));
    }
}

/* ======================================================================== */

int main()
{
    stdio_init_all();
    sleep_ms(2000);                     /* time to open the Serial Monitor */

    reedInit();
    sw_i2c::init();
    const bool have_imu = imu::begin();

    std::printf("# projectile: servo GP%u, reed GP%u, IMU SDA GP%u SCL GP%u\n",
                kServoPin, kReedPin, kImuSda, kImuScl);
    if (have_imu) {
        std::printf("# IMU found at 0x%02X\n", imu::addr);
    } else {
        std::printf("# IMU NOT found -- check SDA pin 29, SCL pin 27, 3V3, GND."
                    " Shots still run, without data.\n");
    }

    servo_init(kServoPin);
    moveServo(kRestAngle);
    waitMs(kStartDelayMs);

    if (have_imu) {
        std::printf("t_ms,shot,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps\n");
    }

    int n = 1;
    for (float angle : kShotAngles) {
        if (g_stopped) {
            break;
        }
        shot(n++, angle, have_imu);
    }

    moveServo(kRestAngle);
    if (g_stopped) {
        std::printf("# Stopped by reed switch (GP%u) - at %.0f\n", kReedPin,
                    kRestAngle);
    } else {
        std::printf("# Done - holding at %.0f\n", kRestAngle);
    }

    while (true) {                      /* hold the servo position */
        sleep_ms(1000);
    }
}
