#pragma once

#include <cstdint>
#include <optional>
#include <random>

#include "FrameBuffer.hpp"
#include "Mood.hpp"

namespace smartface {

struct Gaze {
    float x = 0.0f;
    float y = 0.0f;
};

struct EyePose {
    float centerX = 0.0f;
    float centerY = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float radius = 0.0f;
    float lidInner = 0.0f;
    float lidOuter = 0.0f;
    float lidBottom = 0.0f;
};

struct FacePose {
    EyePose left;
    EyePose right;
};

class FaceAnimator {
public:
    explicit FaceAnimator(uint32_t seed);

    FacePose update(float dtSeconds, Expression expression, std::optional<Gaze> gaze);

private:
    struct Shape {
        float width;
        float height;
        float radius;
        float lidInner;
        float lidOuter;
        float lidBottom;
        float gazeBiasY;
    };

    static Shape shapeFor(Expression expression);
    static bool canBlink(Expression expression);

    float randomRange(float low, float high);
    float updateBlink(Expression expression);
    Gaze updateIdleGaze();
    EyePose makeEye(float baseCenterX, const Gaze& gaze, float sizeScale, float openness) const;

    std::minstd_rand rng_;
    float time_ = 0.0f;

    Shape shape_;
    Gaze gaze_{};
    Gaze idleTarget_{};
    float nextSaccadeAt_ = 0.0f;

    float nextBlinkAt_ = 0.0f;
    float blinkStartedAt_ = -1.0f;
    bool doubleBlinkPending_ = false;
};

void renderFace(gfx::FrameBuffer& frame, const FacePose& pose);

}  // namespace smartface
