#pragma once

#include "Mpu6050.hpp"
#include "Sh1106.hpp"
#include "driver/i2c_master.h"
#include "esp_err.h"

#include "FaceTask.hpp"
#include "MotionTask.hpp"

namespace smartface {

class App {
public:
    App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    esp_err_t start();

private:
    esp_err_t initI2cBus();
    esp_err_t initDisplay();

    i2c_master_bus_handle_t i2cBus_ = nullptr;
    drivers::Mpu6050 imu_;
    drivers::Sh1106 display_;
    MotionTask motionTask_;
    FaceTask faceTask_;
};

}  // namespace smartface
