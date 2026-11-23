#pragma once

#include "MotionState.hpp"

namespace smartface {

class MotionEstimator {
public:
    struct Config {
        float filterTimeConstantS = 0.25f;
        float accelTrustBandG = 0.25f;
        float shakeDeadbandG = 0.35f;
        float shakeFullScaleG = 1.2f;
        float shakeAttackPerS = 3.5f;
        float shakeDecayPerS = 0.7f;
        float stillGyroDps = 4.0f;
        float stillAccelG = 0.05f;
        float upsideDownEnterG = -0.6f;
        float upsideDownExitG = -0.3f;
    };

    MotionEstimator() : MotionEstimator(Config{}) {}
    explicit MotionEstimator(const Config& config) : config_(config) {}

    const MotionState& update(const ImuReading& reading, float dtSeconds);
    void reset();

    const MotionState& state() const { return state_; }

private:
    Config config_;
    MotionState state_{};
    bool hasOrientation_ = false;
    float stillAccumulatorMs_ = 0.0f;
};

}  // namespace smartface
