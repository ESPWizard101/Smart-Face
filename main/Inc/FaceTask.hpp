#pragma once

#include <cstdint>

#include "FrameBuffer.hpp"
#include "Sh1106.hpp"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "Face.hpp"
#include "Mood.hpp"
#include "TaskConfig.hpp"

namespace smartface {

class MotionTask;

class FaceTask {
public:
    struct Config {
        uint32_t framePeriodMs = 33;
        uint8_t contrast = 0xCF;
        uint8_t sleepContrast = 0x08;
    };

    FaceTask(drivers::Sh1106& display, const MotionTask& motion, uint32_t seed);

    FaceTask(const FaceTask&) = delete;
    FaceTask& operator=(const FaceTask&) = delete;

    esp_err_t start(const Config& config, const TaskConfig& task);

private:
    static void taskEntry(void* context);
    [[noreturn]] void run();

    void applyContrast(Expression expression);

    drivers::Sh1106& display_;
    const MotionTask& motion_;
    Config config_{};
    MoodEngine mood_;
    FaceAnimator animator_;
    gfx::FrameBuffer frame_;
    bool dimmed_ = false;
    TaskHandle_t handle_ = nullptr;

    static_assert(gfx::FrameBuffer::kSize == drivers::Sh1106::kFrameSize, "frame buffer does not match the panel");
};

}  // namespace smartface
