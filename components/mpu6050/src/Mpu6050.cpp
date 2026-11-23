#include "Mpu6050.hpp"

#include <algorithm>

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace drivers {
namespace {

constexpr const char* TAG = "mpu6050";

/// Register map, see "MPU-6000/MPU-6050 Register Map and Descriptions" rev 4.2.
namespace reg {
constexpr uint8_t kSmplrtDiv = 0x19;
constexpr uint8_t kConfig = 0x1A;
constexpr uint8_t kGyroConfig = 0x1B;
constexpr uint8_t kAccelConfig = 0x1C;
constexpr uint8_t kAccelXoutH = 0x3B;
constexpr uint8_t kPwrMgmt1 = 0x6B;
constexpr uint8_t kWhoAmI = 0x75;
}  // namespace reg

constexpr uint8_t kPwrMgmt1DeviceReset = 0x80;
constexpr uint8_t kPwrMgmt1Sleep = 0x40;
constexpr uint8_t kPwrMgmt1ClockPllGyroX = 0x01;

constexpr int kTransferTimeoutMs = 50;
constexpr uint32_t kResetDelayMs = 100;
constexpr uint32_t kWakeDelayMs = 10;

/// Spread of the gyro readings during calibration above which we assume the device moved.
constexpr float kCalibrationMaxSpreadDps = 10.0f;

int16_t toInt16(uint8_t high, uint8_t low) {
    return static_cast<int16_t>((static_cast<uint16_t>(high) << 8) | low);
}

}  // namespace

Mpu6050::~Mpu6050() {
    if (device_ != nullptr) {
        i2c_master_bus_rm_device(device_);
    }
}

esp_err_t Mpu6050::init(i2c_master_bus_handle_t bus, const Config& config) {
    ESP_RETURN_ON_FALSE(bus != nullptr, ESP_ERR_INVALID_ARG, TAG, "bus handle is null");
    initialized_ = false;

    if (device_ == nullptr) {
        i2c_device_config_t deviceConfig = {};
        deviceConfig.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        deviceConfig.device_address = config.address;
        deviceConfig.scl_speed_hz = config.sclSpeedHz;
        ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &deviceConfig, &device_), TAG,
                            "failed to add device 0x%02X", config.address);
    }

    uint8_t whoAmI = 0;
    ESP_RETURN_ON_ERROR(readRegisters(reg::kWhoAmI, &whoAmI, 1), TAG,
                        "no answer at 0x%02X (check wiring, pull-ups and AD0)", config.address);
    // WHO_AM_I is 0x68 on a genuine part. Clones and MPU-6500 based boards report other
    // values but are register compatible for everything this driver uses.
    if (whoAmI == 0x00 || whoAmI == 0xFF) {
        ESP_LOGE(TAG, "invalid WHO_AM_I 0x%02X, bus is not working", whoAmI);
        return ESP_ERR_INVALID_RESPONSE;
    }
    if (whoAmI != 0x68) {
        ESP_LOGW(TAG, "unexpected WHO_AM_I 0x%02X (expected 0x68), continuing", whoAmI);
    }

    ESP_RETURN_ON_ERROR(writeRegister(reg::kPwrMgmt1, kPwrMgmt1DeviceReset), TAG, "reset failed");
    vTaskDelay(pdMS_TO_TICKS(kResetDelayMs));

    /*  
     * Leave sleep mode and clock from the X gyro PLL, which is more stable than the
     * internal 8 MHz oscillator.
     */
     ESP_RETURN_ON_ERROR(writeRegister(reg::kPwrMgmt1, kPwrMgmt1ClockPllGyroX), TAG, "wake failed");
    vTaskDelay(pdMS_TO_TICKS(kWakeDelayMs));

    uint8_t pwrMgmt1 = 0;
    ESP_RETURN_ON_ERROR(readRegisters(reg::kPwrMgmt1, &pwrMgmt1, 1), TAG, "PWR_MGMT_1 read failed");
    ESP_RETURN_ON_FALSE((pwrMgmt1 & kPwrMgmt1Sleep) == 0, ESP_ERR_INVALID_STATE, TAG,
                        "device did not leave sleep mode (PWR_MGMT_1=0x%02X)", pwrMgmt1);

    const auto accelRange = static_cast<uint8_t>(config.accelRange);
    const auto gyroRange = static_cast<uint8_t>(config.gyroRange);
    ESP_RETURN_ON_ERROR(writeRegister(reg::kConfig, static_cast<uint8_t>(config.bandwidth)), TAG, "CONFIG");
    ESP_RETURN_ON_ERROR(writeRegister(reg::kSmplrtDiv, config.sampleRateDivider), TAG, "SMPLRT_DIV");
    ESP_RETURN_ON_ERROR(writeRegister(reg::kGyroConfig, static_cast<uint8_t>(gyroRange << 3)), TAG, "GYRO_CONFIG");
    ESP_RETURN_ON_ERROR(writeRegister(reg::kAccelConfig, static_cast<uint8_t>(accelRange << 3)), TAG, "ACCEL_CONFIG");

    // Each range step doubles the full scale and halves the sensitivity.
    accelLsbPerG_ = 16384.0f / static_cast<float>(1u << accelRange);
    gyroLsbPerDps_ = 131.0f / static_cast<float>(1u << gyroRange);
    samplePeriodMs_ = std::max<uint32_t>(1, 1u + config.sampleRateDivider);

    initialized_ = true;
    ESP_LOGI(TAG, "ready at 0x%02X: +/-%dg, +/-%d dps, %lu Hz", config.address, 2 << accelRange,
             250 << gyroRange, static_cast<unsigned long>(1000 / samplePeriodMs_));
    return ESP_OK;
}

esp_err_t Mpu6050::read(Sample& out) {
    RawSample raw{};
    ESP_RETURN_ON_ERROR(readRaw(raw), TAG, "sample read failed");

    out.accel = {raw.accel[0] / accelLsbPerG_, raw.accel[1] / accelLsbPerG_, raw.accel[2] / accelLsbPerG_};
    out.gyro = {raw.gyro[0] / gyroLsbPerDps_ - gyroBias_.x, raw.gyro[1] / gyroLsbPerDps_ - gyroBias_.y,
                raw.gyro[2] / gyroLsbPerDps_ - gyroBias_.z};
    // Datasheet formula: T = raw / 340 + 36.53 degrees Celsius.
    out.temperatureC = raw.temperature / 340.0f + 36.53f;
    return ESP_OK;
}

esp_err_t Mpu6050::calibrateGyro(uint16_t sampleCount) {
    ESP_RETURN_ON_FALSE(initialized_, ESP_ERR_INVALID_STATE, TAG, "not initialized");
    ESP_RETURN_ON_FALSE(sampleCount > 0, ESP_ERR_INVALID_ARG, TAG, "sampleCount must be > 0");

    float sum[3] = {};
    float minimum[3] = {1e9f, 1e9f, 1e9f};
    float maximum[3] = {-1e9f, -1e9f, -1e9f};

    for (uint16_t i = 0; i < sampleCount; ++i) {
        RawSample raw{};
        ESP_RETURN_ON_ERROR(readRaw(raw), TAG, "calibration read failed");
        for (int axis = 0; axis < 3; ++axis) {
            const float dps = raw.gyro[axis] / gyroLsbPerDps_;
            sum[axis] += dps;
            minimum[axis] = std::min(minimum[axis], dps);
            maximum[axis] = std::max(maximum[axis], dps);
        }
        vTaskDelay(pdMS_TO_TICKS(samplePeriodMs_));
    }

    for (int axis = 0; axis < 3; ++axis) {
        if (maximum[axis] - minimum[axis] > kCalibrationMaxSpreadDps) {
            ESP_LOGW(TAG, "gyro calibration rejected: device moved (axis %d spread %.1f dps)", axis,
                     static_cast<double>(maximum[axis] - minimum[axis]));
            return ESP_ERR_INVALID_STATE;
        }
    }

    const auto count = static_cast<float>(sampleCount);
    gyroBias_ = {sum[0] / count, sum[1] / count, sum[2] / count};
    ESP_LOGI(TAG, "gyro bias: %.2f %.2f %.2f dps", static_cast<double>(gyroBias_.x),
             static_cast<double>(gyroBias_.y), static_cast<double>(gyroBias_.z));
    return ESP_OK;
}

esp_err_t Mpu6050::readRaw(RawSample& raw) {
    ESP_RETURN_ON_FALSE(device_ != nullptr, ESP_ERR_INVALID_STATE, TAG, "not attached to a bus");

    /* 
     * ACCEL_XOUT_H..GYRO_ZOUT_L: 14 consecutive big-endian registers. The register pointer
     * auto-increments, so one burst returns a coherent snapshot of all axes.
     */
     uint8_t data[14];
    ESP_RETURN_ON_ERROR(readRegisters(reg::kAccelXoutH, data, sizeof(data)), TAG, "burst read failed");

    raw.accel[0] = toInt16(data[0], data[1]);
    raw.accel[1] = toInt16(data[2], data[3]);
    raw.accel[2] = toInt16(data[4], data[5]);
    raw.temperature = toInt16(data[6], data[7]);
    raw.gyro[0] = toInt16(data[8], data[9]);
    raw.gyro[1] = toInt16(data[10], data[11]);
    raw.gyro[2] = toInt16(data[12], data[13]);
    return ESP_OK;
}

esp_err_t Mpu6050::writeRegister(uint8_t reg, uint8_t value) {
    const uint8_t frame[2] = {reg, value};
    return i2c_master_transmit(device_, frame, sizeof(frame), kTransferTimeoutMs);
}

esp_err_t Mpu6050::readRegisters(uint8_t reg, uint8_t* data, size_t length) {
    // Write the register address, then repeated START and read `length` bytes.
    return i2c_master_transmit_receive(device_, &reg, 1, data, length, kTransferTimeoutMs);
}

}  // namespace drivers
