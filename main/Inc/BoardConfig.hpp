#pragma once

#include <cstdint>

#include "driver/gpio.h"
#include "driver/i2c_types.h"
#include "driver/spi_common.h"
#include "freertos/FreeRTOS.h"
#include "sdkconfig.h"

#include "MotionState.hpp"
#include "TaskConfig.hpp"

namespace smartface::board {

inline constexpr i2c_port_num_t kI2cPort = I2C_NUM_0;
inline constexpr gpio_num_t kI2cSda = static_cast<gpio_num_t>(CONFIG_SMARTFACE_I2C_SDA_GPIO);
inline constexpr gpio_num_t kI2cScl = static_cast<gpio_num_t>(CONFIG_SMARTFACE_I2C_SCL_GPIO);
inline constexpr uint32_t kI2cFrequencyHz = CONFIG_SMARTFACE_I2C_FREQ_HZ;
#ifdef CONFIG_SMARTFACE_I2C_INTERNAL_PULLUPS
inline constexpr bool kI2cInternalPullups = true;
#else
inline constexpr bool kI2cInternalPullups = false;
#endif

#ifdef CONFIG_SMARTFACE_MPU6050_AD0_HIGH
inline constexpr uint8_t kMpu6050Address = 0x69;
#else
inline constexpr uint8_t kMpu6050Address = 0x68;
#endif

inline constexpr spi_host_device_t kDisplaySpiHost = SPI3_HOST;
inline constexpr gpio_num_t kDisplaySclk = static_cast<gpio_num_t>(CONFIG_SMARTFACE_DISPLAY_SCLK_GPIO);
inline constexpr gpio_num_t kDisplayMosi = static_cast<gpio_num_t>(CONFIG_SMARTFACE_DISPLAY_MOSI_GPIO);
inline constexpr gpio_num_t kDisplayCs = static_cast<gpio_num_t>(CONFIG_SMARTFACE_DISPLAY_CS_GPIO);
inline constexpr gpio_num_t kDisplayDc = static_cast<gpio_num_t>(CONFIG_SMARTFACE_DISPLAY_DC_GPIO);
inline constexpr gpio_num_t kDisplayReset = static_cast<gpio_num_t>(CONFIG_SMARTFACE_DISPLAY_RESET_GPIO);
inline constexpr int kDisplayClockHz = CONFIG_SMARTFACE_DISPLAY_CLOCK_HZ;
#ifdef CONFIG_SMARTFACE_DISPLAY_ROTATE_180
inline constexpr bool kDisplayRotate180 = true;
#else
inline constexpr bool kDisplayRotate180 = false;
#endif
inline constexpr uint8_t kDisplayContrast = CONFIG_SMARTFACE_DISPLAY_CONTRAST;
inline constexpr uint8_t kDisplaySleepContrast = CONFIG_SMARTFACE_DISPLAY_SLEEP_CONTRAST;

// Adjust this mapping if the sensor is mounted in a different orientation.
inline Vec3 sensorToFaceFrame(float x, float y, float z) {
    return {x, y, z};
}

inline constexpr TaskConfig kMotionTask{"motion", 4096, 6, 1};
inline constexpr TaskConfig kFaceTask{"face", 4096, 5, 1};

inline constexpr uint32_t kMotionPeriodMs = 10;  // 100 Hz, matches the IMU output data rate
inline constexpr uint32_t kFramePeriodMs = 33;   // ~30 fps

}  // namespace smartface::board
