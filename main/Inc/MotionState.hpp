#pragma once

#include <cstdint>

namespace smartface {

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct ImuReading {
    Vec3 accelG;
    Vec3 gyroDps;
};

struct MotionState {
    float rollDeg = 0.0f;
    float pitchDeg = 0.0f;
    float shake = 0.0f;
    uint32_t stillMs = 0;
    bool upsideDown = false;
    bool sensorOk = false;
};

}  // namespace smartface
