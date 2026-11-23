#pragma once

#include <cstddef>
#include <cstdint>

#include "driver/i2c_master.h"
#include "esp_err.h"

namespace drivers {

struct Vector3f {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

class Mpu6050 {
public:
    static constexpr uint8_t kDefaultAddress = 0x68;
    static constexpr uint8_t kAlternateAddress = 0x69;

    enum class AccelRange : uint8_t { G2 = 0, G4 = 1, G8 = 2, G16 = 3 };
    enum class GyroRange : uint8_t { Dps250 = 0, Dps500 = 1, Dps1000 = 2, Dps2000 = 3 };

    enum class Bandwidth : uint8_t { Hz260 = 0, Hz184 = 1, Hz94 = 2, Hz44 = 3, Hz21 = 4, Hz10 = 5, Hz5 = 6 };

    struct Config {
        uint8_t address = kDefaultAddress;
        uint32_t sclSpeedHz = 400000;
        AccelRange accelRange = AccelRange::G2;
        GyroRange gyroRange = GyroRange::Dps250;
        Bandwidth bandwidth = Bandwidth::Hz44;
        uint8_t sampleRateDivider = 9;
    };

    struct Sample {
        Vector3f accel;
        Vector3f gyro;
        float temperatureC = 0.0f;
    };

    Mpu6050() = default;
    ~Mpu6050();

    Mpu6050(const Mpu6050&) = delete;
    Mpu6050& operator=(const Mpu6050&) = delete;

    esp_err_t init(i2c_master_bus_handle_t bus, const Config& config);

    esp_err_t read(Sample& out);

    esp_err_t calibrateGyro(uint16_t sampleCount);

    void setGyroBias(const Vector3f& bias) { gyroBias_ = bias; }
    const Vector3f& gyroBias() const { return gyroBias_; }

    bool isInitialized() const { return initialized_; }

    uint32_t samplePeriodMs() const { return samplePeriodMs_; }

private:
    struct RawSample {
        int16_t accel[3];
        int16_t temperature;
        int16_t gyro[3];
    };

    esp_err_t writeRegister(uint8_t reg, uint8_t value);
    esp_err_t readRegisters(uint8_t reg, uint8_t* data, size_t length);
    esp_err_t readRaw(RawSample& raw);

    i2c_master_dev_handle_t device_ = nullptr;
    bool initialized_ = false;
    float accelLsbPerG_ = 16384.0f;
    float gyroLsbPerDps_ = 131.0f;
    uint32_t samplePeriodMs_ = 10;
    Vector3f gyroBias_{};
};

}  // namespace drivers
