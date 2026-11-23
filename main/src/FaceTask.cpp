#include "FaceTask.hpp"

#include <algorithm>
#include <cmath>
#include <optional>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "MotionTask.hpp"

namespace smartface {
namespace {

constexpr const char* TAG = "face";

constexpr float kTrackEnterDeg = 10.0f;
constexpr float kTrackExitDeg = 6.0f;
constexpr float kRollFullScaleDeg = 35.0f;
constexpr float kPitchFullScaleDeg = 30.0f;

constexpr int kLogEveryNthWriteError = 100;

class TiltTracker {
public:
    std::optional<Gaze> update(const MotionState& motion, Expression expression) {
        if (!motion.sensorOk || motion.upsideDown || expression == Expression::Dizzy ||
            expression == Expression::Asleep) {
            tracking_ = false;
            return std::nullopt;
        }
        const float tilt = std::max(std::fabs(motion.rollDeg), std::fabs(motion.pitchDeg));
        tracking_ = tracking_ ? tilt > kTrackExitDeg : tilt > kTrackEnterDeg;
        if (!tracking_) {
            return std::nullopt;
        }
        return Gaze{std::clamp(-motion.rollDeg / kRollFullScaleDeg, -1.0f, 1.0f),
                    std::clamp(motion.pitchDeg / kPitchFullScaleDeg, -1.0f, 1.0f)};
    }

private:
    bool tracking_ = false;
};

}  // namespace

FaceTask::FaceTask(drivers::Sh1106& display, const MotionTask& motion, uint32_t seed)
    : display_(display), motion_(motion), mood_(seed), animator_(seed ^ 0x9E3779B9u) {}

esp_err_t FaceTask::start(const Config& config, const TaskConfig& task) {
    ESP_RETURN_ON_FALSE(handle_ == nullptr, ESP_ERR_INVALID_STATE, TAG, "already started");
    ESP_RETURN_ON_FALSE(display_.isInitialized(), ESP_ERR_INVALID_STATE, TAG, "display not initialized");

    config_ = config;
    config_.framePeriodMs = std::max<uint32_t>(1, config_.framePeriodMs);

    const BaseType_t created = xTaskCreatePinnedToCore(&FaceTask::taskEntry, task.name, task.stackBytes, this,
                                                       task.priority, &handle_, task.core);
    ESP_RETURN_ON_FALSE(created == pdPASS, ESP_ERR_NO_MEM, TAG, "task creation failed");
    return ESP_OK;
}

void FaceTask::taskEntry(void* context) {
    static_cast<FaceTask*>(context)->run();
}

void FaceTask::run() {
    TiltTracker tiltTracker;
    Expression shown = mood_.current();
    int writeErrors = 0;

    const TickType_t period = std::max<TickType_t>(1, pdMS_TO_TICKS(config_.framePeriodMs));
    TickType_t wakeTime = xTaskGetTickCount();
    int64_t lastFrameUs = esp_timer_get_time();
    int64_t moodRemainderUs = 0;

    for (;;) {
        vTaskDelayUntil(&wakeTime, period);

        const int64_t nowUs = esp_timer_get_time();
        const int64_t elapsedUs = nowUs - lastFrameUs;
        lastFrameUs = nowUs;

        moodRemainderUs += elapsedUs;
        const auto elapsedMs = static_cast<uint32_t>(moodRemainderUs / 1000);
        moodRemainderUs -= static_cast<int64_t>(elapsedMs) * 1000;

        MotionState motion{};
        if (!motion_.latest(motion)) {
            motion = MotionState{};
        }

        const Expression expression = mood_.update(motion, elapsedMs);
        if (expression != shown) {
            ESP_LOGI(TAG, "%s -> %s", toString(shown), toString(expression));
            shown = expression;
        }
        applyContrast(expression);

        const FacePose pose =
            animator_.update(static_cast<float>(elapsedUs) * 1e-6f, expression, tiltTracker.update(motion, expression));
        renderFace(frame_, pose);

        if (display_.writeFrame(frame_.data()) != ESP_OK) {
            if (writeErrors++ % kLogEveryNthWriteError == 0) {
                ESP_LOGW(TAG, "display write failed (%d so far)", writeErrors);
            }
        }
    }
}

void FaceTask::applyContrast(Expression expression) {
    const bool shouldDim = expression == Expression::Asleep;
    if (shouldDim == dimmed_) {
        return;
    }
    if (display_.setContrast(shouldDim ? config_.sleepContrast : config_.contrast) == ESP_OK) {
        dimmed_ = shouldDim;
    }
}

}  // namespace smartface
