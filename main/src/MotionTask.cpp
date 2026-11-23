#include "MotionTask.hpp"

#include <algorithm>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "BoardConfig.hpp"

namespace smartface {
namespace {

constexpr const char* TAG = "motion";

constexpr uint16_t kCalibrationSamples = 100;
constexpr int kMaxConsecutiveErrors = 10;
constexpr uint32_t kRetryIntervalMs = 1000;

constexpr uint32_t kBiasTrackingAfterStillMs = 2000;
constexpr float kBiasTrackingGain = 0.002f;

}  // namespace

esp_err_t MotionTask::start(i2c_master_bus_handle_t bus, const drivers::Mpu6050::Config& imuConfig,
                            const TaskConfig& task, uint32_t periodMs) {
    ESP_RETURN_ON_FALSE(handle_ == nullptr, ESP_ERR_INVALID_STATE, TAG, "already started");
    ESP_RETURN_ON_FALSE(bus != nullptr, ESP_ERR_INVALID_ARG, TAG, "bus handle is null");

    bus_ = bus;
    imuConfig_ = imuConfig;
    periodMs_ = std::max<uint32_t>(1, periodMs);

    mailbox_ = xQueueCreate(1, sizeof(MotionState));
    ESP_RETURN_ON_FALSE(mailbox_ != nullptr, ESP_ERR_NO_MEM, TAG, "mailbox allocation failed");
    publish(MotionState{});

    const BaseType_t created =
        xTaskCreatePinnedToCore(&MotionTask::taskEntry, task.name, task.stackBytes, this, task.priority, &handle_,
                                task.core);
    ESP_RETURN_ON_FALSE(created == pdPASS, ESP_ERR_NO_MEM, TAG, "task creation failed");
    return ESP_OK;
}

bool MotionTask::latest(MotionState& out) const {
    return mailbox_ != nullptr && xQueuePeek(mailbox_, &out, 0) == pdTRUE;
}

void MotionTask::taskEntry(void* context) {
    static_cast<MotionTask*>(context)->run();
}

void MotionTask::run() {
    int consecutiveErrors = 0;
    bool sensorUp = bringUpSensor();
    int64_t lastSampleUs = esp_timer_get_time();
    TickType_t wakeTime = xTaskGetTickCount();
    const TickType_t period = std::max<TickType_t>(1, pdMS_TO_TICKS(periodMs_));

    for (;;) {
        if (!sensorUp) {
            vTaskDelay(pdMS_TO_TICKS(kRetryIntervalMs));
            sensorUp = bringUpSensor();
            consecutiveErrors = 0;
            lastSampleUs = esp_timer_get_time();
            wakeTime = xTaskGetTickCount();
            continue;
        }

        vTaskDelayUntil(&wakeTime, period);

        drivers::Mpu6050::Sample sample{};
        if (imu_.read(sample) != ESP_OK) {
            if (++consecutiveErrors >= kMaxConsecutiveErrors) {
                ESP_LOGE(TAG, "MPU6050 lost, retrying every %lu ms", static_cast<unsigned long>(kRetryIntervalMs));
                publish(MotionState{});
                sensorUp = false;
            }
            continue;
        }
        consecutiveErrors = 0;

        const int64_t nowUs = esp_timer_get_time();
        const float dt = static_cast<float>(nowUs - lastSampleUs) * 1e-6f;
        lastSampleUs = nowUs;

        ImuReading reading;
        reading.accelG = board::sensorToFaceFrame(sample.accel.x, sample.accel.y, sample.accel.z);
        reading.gyroDps = board::sensorToFaceFrame(sample.gyro.x, sample.gyro.y, sample.gyro.z);

        const MotionState& state = estimator_.update(reading, dt);
        publish(state);

        if (state.stillMs > kBiasTrackingAfterStillMs) {
            drivers::Vector3f bias = imu_.gyroBias();
            bias.x += kBiasTrackingGain * sample.gyro.x;
            bias.y += kBiasTrackingGain * sample.gyro.y;
            bias.z += kBiasTrackingGain * sample.gyro.z;
            imu_.setGyroBias(bias);
        }
    }
}

bool MotionTask::bringUpSensor() {
    if (imu_.init(bus_, imuConfig_) != ESP_OK) {
        publish(MotionState{});
        return false;
    }

    if (imu_.calibrateGyro(kCalibrationSamples) != ESP_OK) {
        ESP_LOGW(TAG, "gyro calibration skipped, relying on online bias tracking");
    }
    estimator_.reset();
    return true;
}

void MotionTask::publish(const MotionState& state) {
    xQueueOverwrite(mailbox_, &state);
}

}  // namespace smartface
