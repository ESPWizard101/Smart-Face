#include "MotionEstimator.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace smartface {
namespace {

constexpr float kRadToDeg = 57.29577951f;

float wrapDegrees(float angle) {
    while (angle > 180.0f) {
        angle -= 360.0f;
    }
    while (angle <= -180.0f) {
        angle += 360.0f;
    }
    return angle;
}

float fuse(float previous, float rateDps, float measured, float dt, float accelWeight) {
    const float predicted = wrapDegrees(previous + rateDps * dt);
    const float error = wrapDegrees(measured - predicted);
    return wrapDegrees(predicted + accelWeight * error);
}

float magnitude(const Vec3& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

}  // namespace

const MotionState& MotionEstimator::update(const ImuReading& reading, float dtSeconds) {
    const Vec3& a = reading.accelG;
    const Vec3& g = reading.gyroDps;
    const float dt = std::clamp(dtSeconds, 0.0f, 0.2f);

    const float accelMagnitude = magnitude(a);
    const float dynamicAccel = std::fabs(accelMagnitude - 1.0f);

    /*
     * With the face frame defined in BoardConfig.hpp the accelerometer reads +1 g on Y when
     * the face is upright. Roll turns about Z (screen normal), pitch about X.
     */
    const float accelRoll = std::atan2(a.x, a.y) * kRadToDeg;
    const float accelPitch = std::atan2(-a.z, std::sqrt(a.x * a.x + a.y * a.y)) * kRadToDeg;

    if (!hasOrientation_) {
        state_.rollDeg = accelRoll;
        state_.pitchDeg = accelPitch;
        hasOrientation_ = true;
    } else {
        const bool accelTrusted = dynamicAccel < config_.accelTrustBandG;
        const float weight = accelTrusted ? dt / (config_.filterTimeConstantS + dt) : 0.0f;
        state_.rollDeg = fuse(state_.rollDeg, g.z, accelRoll, dt, weight);
        state_.pitchDeg = fuse(state_.pitchDeg, g.x, accelPitch, dt, weight);
    }

    if (state_.upsideDown) {
        state_.upsideDown = a.y < config_.upsideDownExitG;
    } else {
        state_.upsideDown = a.y < config_.upsideDownEnterG;
    }

    const float intensity = std::clamp((dynamicAccel - config_.shakeDeadbandG) /
                                           (config_.shakeFullScaleG - config_.shakeDeadbandG),
                                       0.0f, 1.0f);
    state_.shake += intensity * config_.shakeAttackPerS * dt;
    state_.shake -= config_.shakeDecayPerS * dt;
    state_.shake = std::clamp(state_.shake, 0.0f, 1.0f);

    const bool still = magnitude(g) < config_.stillGyroDps && dynamicAccel < config_.stillAccelG;
    if (still) {
        stillAccumulatorMs_ = std::min(stillAccumulatorMs_ + dt * 1000.0f,
                                       static_cast<float>(std::numeric_limits<uint32_t>::max() / 2));
    } else {
        stillAccumulatorMs_ = 0.0f;
    }
    state_.stillMs = static_cast<uint32_t>(stillAccumulatorMs_);

    state_.sensorOk = true;
    return state_;
}

void MotionEstimator::reset() {
    state_ = MotionState{};
    hasOrientation_ = false;
    stillAccumulatorMs_ = 0.0f;
}

}  // namespace smartface
