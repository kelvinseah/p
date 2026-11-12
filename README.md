# projectile

Servo launcher with a reed-switch stop and IMU logging for Tracker video analysis.
**All of the project's code is in `app/main.cpp`.** Everything else is the ENG1104
course material, left exactly as provided (only the `exNN_` prefixes were dropped).

```
projectile/
  CMakeLists.txt        builds app/ and every course exercise
  app/main.cpp          <- THE project program (target: projectile)
  app/CMakeLists.txt
  board.h  common/      course pin map and drivers (unchanged)
  w08/ w09/ w10/        course exercises (unchanged)
  pico_sdk_import.cmake
```

## Wiring (bare Pico WH on perfboard)

| Component | Pin ID & GP |
|---|---|
| Power | VSYS (39), GND (38) |
| Servo | VSYS (39), GND (33), SIG (34 = GP28) |
| IMU | 3V3 OUT (36), GND (28), SDA (29 = GP22), SCL (27 = GP21) |
| Reed switch | 3V3 OUT (36), GND (23), SIG (21 = GP16) |

GP22 + GP21 belong to two different I2C controllers, so the Pico's hardware I2C
can't use them. `main.cpp` drives the IMU bus in software on those pins instead.

## What `projectile` does

1. Starts at 0°, waits 1 s.
2. For each angle in `kShotAngles` (90°, then 0°): prints `# LAUNCH n`, moves the
   servo, logs the IMU at 100 samples/s for 1 s.
3. Holds at 0°.

A magnet on the reed switch (held ~30 ms) stops it at any point and returns to 0°.

All settings are at the top of `main.cpp`: pins, shot angles, zero offset,
logging time and rate, and an optional buzzer (`kBuzzerPin`, off by default —
never 21 or 22).

## Serial output

Lines starting with `#` are messages. Everything else is CSV:

```
t_ms,shot,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps
```

`t_ms` counts from that shot's LAUNCH line, so `t_ms = 0` is the video frame where
the servo arm starts to move. Paste the CSV into Excel next to Tracker's export.

## Build and flash

1. VS Code → **File → Open Folder** → `projectile`; accept the Pico project import
   (SDK 2.3.1, board `pico_w`).
2. CMake panel → **Refresh**, then build target **`projectile`**.
3. Hold BOOTSEL, plug in, drag `build/app/projectile.uf2` onto RPI-RP2.
4. Open the Serial Monitor within 2 s to catch the start.

Build only the project: `-DENG1104_BUILD_W08=OFF -DENG1104_BUILD_W09=OFF -DENG1104_BUILD_W10=OFF`.

## Tracker

1. Film side-on in slow motion (120/240 fps), with a ruler in the plane of flight.
2. In Tracker, **set the real frame rate in Clip Settings** (slow-mo files often say 30).
3. Calibration stick on the ruler, origin at the release point.
4. Track the projectile → launch speed v₀ = √(vx² + vy²), angle θ = tan⁻¹(vy/vx) from
   the first frames; check ay ≈ −9.8 m/s².
5. Compare the measured range with R = v₀² sin 2θ / g.

## Course exercises

The `w08`, `w09` and `w10` programs use the course pins (IMU on GP4/GP5, servo
GP12, buzzer GP22, buttons GP20/21), not the perfboard wiring above.
