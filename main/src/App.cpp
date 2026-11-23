#include "App.hpp"

#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_random.h"

#include "BoardConfig.hpp"

namespace smartface {
namespace {

constexpr const char* TAG = "app";

}  // namespace

App::App() : motionTask_(imu_), faceTask_(display_, motionTask_, esp_random()) {}

esp_err_t App::start() {
    if (const esp_err_t err = initI2cBus(); err != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed (%s), running without motion", esp_err_to_name(err));
    } else {
        drivers::Mpu6050::Config imuConfig;
        imuConfig.address = board::kMpu6050Address;
        imuConfig.sclSpeedHz = board::kI2cFrequencyHz;
        imuConfig.accelRange = drivers::Mpu6050::AccelRange::G4;  // headroom for shake detection
        imuConfig.gyroRange = drivers::Mpu6050::GyroRange::Dps500;
        imuConfig.bandwidth = drivers::Mpu6050::Bandwidth::Hz44;
        imuConfig.sampleRateDivider = 9;  // 100 Hz

        if (const esp_err_t taskErr = motionTask_.start(i2cBus_, imuConfig, board::kMotionTask, board::kMotionPeriodMs);
            taskErr != ESP_OK) {
            ESP_LOGE(TAG, "motion task failed to start (%s)", esp_err_to_name(taskErr));
        }
    }

    ESP_RETURN_ON_ERROR(initDisplay(), TAG, "display init failed");

    const FaceTask::Config faceConfig{board::kFramePeriodMs, board::kDisplayContrast,
                                      board::kDisplaySleepContrast};
    ESP_RETURN_ON_ERROR(faceTask_.start(faceConfig, board::kFaceTask), TAG, "face task failed to start");

    ESP_LOGI(TAG, "Smart Face running");
    return ESP_OK;
}

esp_err_t App::initI2cBus() {
    i2c_master_bus_config_t config = {};
    config.i2c_port = board::kI2cPort;
    config.sda_io_num = board::kI2cSda;
    config.scl_io_num = board::kI2cScl;
    config.clk_source = I2C_CLK_SRC_DEFAULT;
    config.glitch_ignore_cnt = 7;
    config.flags.enable_internal_pullup = board::kI2cInternalPullups;
    return i2c_new_master_bus(&config, &i2cBus_);
}

esp_err_t App::initDisplay() {
    spi_bus_config_t bus = {};
    bus.mosi_io_num = board::kDisplayMosi;
    bus.miso_io_num = GPIO_NUM_NC;
    bus.sclk_io_num = board::kDisplaySclk;
    bus.quadwp_io_num = GPIO_NUM_NC;
    bus.quadhd_io_num = GPIO_NUM_NC;
    bus.max_transfer_sz = static_cast<int>(drivers::Sh1106::kFrameSize);
    ESP_RETURN_ON_ERROR(spi_bus_initialize(board::kDisplaySpiHost, &bus, SPI_DMA_CH_AUTO), TAG,
                        "SPI bus init failed");

    drivers::Sh1106::Config config;
    config.host = board::kDisplaySpiHost;
    config.csPin = board::kDisplayCs;
    config.dcPin = board::kDisplayDc;
    config.resetPin = board::kDisplayReset;
    config.clockHz = board::kDisplayClockHz;
    config.rotate180 = board::kDisplayRotate180;
    config.contrast = board::kDisplayContrast;
    return display_.init(config);
}

}  // namespace smartface
