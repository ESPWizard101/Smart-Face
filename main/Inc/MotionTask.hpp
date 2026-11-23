#pragma once

#include "Mpu6050.hpp"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "MotionEstimator.hpp"
#include "MotionState.hpp"
#include "TaskConfig.hpp"

namespace smartface {

class MotionTask {
public:
    explicit MotionTask(drivers::Mpu6050& imu) : imu_(imu) {}

    MotionTask(const MotionTask&) = delete;
    MotionTask& operator=(const MotionTask&) = delete;

    esp_err_t start(i2c_master_bus_handle_t bus, const drivers::Mpu6050::Config& imuConfig,
                    const TaskConfig& task, uint32_t periodMs);

    bool latest(MotionState& out) const;

private:
    static void taskEntry(void* context);
    [[noreturn]] void run();

    bool bringUpSensor();
    void publish(const MotionState& state);

    drivers::Mpu6050& imu_;
    i2c_master_bus_handle_t bus_ = nullptr;
    drivers::Mpu6050::Config imuConfig_{};
    uint32_t periodMs_ = 10;
    MotionEstimator estimator_;
    QueueHandle_t mailbox_ = nullptr;
    TaskHandle_t handle_ = nullptr;
};

}  // namespace smartface
