# Smart Face

An ESP32 face with animated eyes on a 128x64 SH1106 OLED. An MPU6050 lets it respond to motion.

| What you do                     | What the face does                              |
|---------------------------------|-------------------------------------------------|
| Nothing                         | Blinks, glances around, smiles now and then     |
| Tilt it                         | Looks "downhill", towards the lower side        |
| Shake it                        | Dizzy spinning eyes, then angry for a moment    |
| Turn it upside down             | Surprised, then sad if you leave it like that   |
| Leave it still for 45 s / 2 min | Sleepy, then asleep (display dims)              |
| Pick it up while asleep         | Surprised, then happy                           |

Without the MPU6050, the face idles and retries the sensor once per second.

## Hardware

- ESP32 dev board (ESP32-WROOM)
- SH1106 128x64 OLED, 4-wire SPI (7 pins: GND VCC D0 D1 RES DC CS)
- MPU6050 breakout (GY-521 or similar)

### Wiring (defaults)

| Signal        | ESP32 GPIO | Module pin       |
|---------------|-----------:|------------------|
| I2C SDA       | 21         | MPU6050 SDA      |
| I2C SCL       | 22         | MPU6050 SCL      |
| SPI SCLK      | 18         | OLED D0 / SCK    |
| SPI MOSI      | 23         | OLED D1 / SDA    |
| OLED CS       | 5          | OLED CS          |
| OLED DC       | 16         | OLED DC          |
| OLED RES      | 17         | OLED RES         |
| 3V3 / GND     |            | VCC / GND on both modules |

MPU6050 AD0 to GND (address 0x68). Change pins in `idf.py menuconfig` → **Smart Face**.

Adjust sensor orientation in `board::sensorToFaceFrame()` in
[main/Inc/BoardConfig.hpp](main/Inc/BoardConfig.hpp).

## Build and flash

Requires ESP-IDF v5.5.

```sh
idf.py set-target esp32   # first time only
idf.py build flash monitor
```

The host-side logic tests can be run with:

```sh
cmake -S test/host -B build-host
cmake --build build-host
./build-host/smartface_tests
```
