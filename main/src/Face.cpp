#include "Face.hpp"

#include <algorithm>
#include <cmath>

namespace smartface {
namespace {

constexpr float kPi = 3.14159265f;

constexpr float kScreenWidth = static_cast<float>(gfx::FrameBuffer::kWidth);
constexpr float kScreenHeight = static_cast<float>(gfx::FrameBuffer::kHeight);
constexpr float kEyeSpacing = 52.0f;  // centre to centre
constexpr float kLeftEyeX = kScreenWidth / 2.0f - kEyeSpacing / 2.0f;
constexpr float kRightEyeX = kScreenWidth / 2.0f + kEyeSpacing / 2.0f;
constexpr float kEyeY = kScreenHeight / 2.0f;

constexpr float kGazeRangeX = 16.0f;
constexpr float kGazeRangeY = 10.0f;
constexpr float kPerspective = 0.08f;

constexpr float kShapeTau = 0.08f;
constexpr float kIdleGazeTau = 0.045f;    // quick saccades
constexpr float kTrackedGazeTau = 0.12f;  // following the tilt smoothly

constexpr float kBlinkDuration = 0.16f;
constexpr float kSleepyBlinkDuration = 0.45f;
constexpr float kDoubleBlinkChance = 0.2f;
constexpr float kDoubleBlinkGap = 0.08f;
constexpr float kMinBlinkInterval = 2.0f;
constexpr float kMaxBlinkInterval = 5.5f;

constexpr float kBreathingPeriod = 4.0f;
constexpr float kBreathingAmplitude = 1.5f;
constexpr float kDizzySpinHz = 1.4f;

float smoothingFactor(float dt, float tau) {
    return 1.0f - std::exp(-dt / tau);
}

float approach(float current, float target, float factor) {
    return current + (target - current) * factor;
}

int toPixels(float value) {
    return static_cast<int>(std::lround(value));
}

void renderEye(gfx::FrameBuffer& frame, const EyePose& eye, bool isLeftEye) {
    const int width = std::max(1, toPixels(eye.width));
    const int height = std::max(1, toPixels(eye.height));
    const int x0 = toPixels(eye.centerX - static_cast<float>(width) / 2.0f);
    const int y0 = toPixels(eye.centerY - static_cast<float>(height) / 2.0f);

    frame.fillRoundRect(x0, y0, width, height, toPixels(eye.radius), gfx::Color::On);

    // Upper lid: erase everything above a slanted line. The outer corner of the left eye is
    // on its left side, the outer corner of the right eye on its right side.
    const float innerDrop = eye.lidInner * static_cast<float>(height);
    const float outerDrop = eye.lidOuter * static_cast<float>(height);
    const float dropAtLeft = isLeftEye ? outerDrop : innerDrop;
    const float dropAtRight = isLeftEye ? innerDrop : outerDrop;
    if (dropAtLeft >= 0.5f || dropAtRight >= 0.5f) {
        const int left = x0 - 1;
        const int right = x0 + width;
        const int top = y0 - 1;
        const int yLeft = y0 + toPixels(dropAtLeft);
        const int yRight = y0 + toPixels(dropAtRight);
        frame.fillTriangle(left, top, right, top, right, yRight, gfx::Color::Off);
        frame.fillTriangle(left, top, right, yRight, left, yLeft, gfx::Color::Off);
    }

    // Lower lid: erase a large circle rising from below, which leaves an upward arc.
    if (eye.lidBottom > 0.01f) {
        const int radius = toPixels(eye.width * 0.8f);
        const int centerY = y0 + height + radius - toPixels(eye.lidBottom * static_cast<float>(height));
        frame.fillCircle(toPixels(eye.centerX), centerY, radius, gfx::Color::Off);
    }
}

}  // namespace

FaceAnimator::FaceAnimator(uint32_t seed) : rng_(seed), shape_(shapeFor(Expression::Neutral)) {
    nextBlinkAt_ = randomRange(1.0f, kMaxBlinkInterval);
    nextSaccadeAt_ = randomRange(0.5f, 2.0f);
}

FaceAnimator::Shape FaceAnimator::shapeFor(Expression expression) {
    switch (expression) {
        case Expression::Neutral:   return {36.0f, 36.0f,  9.0f, 0.00f, 0.00f, 0.00f,  0.00f};
        case Expression::Happy:     return {36.0f, 34.0f, 12.0f, 0.00f, 0.00f, 0.45f, -0.10f};
        case Expression::Sleepy:    return {36.0f, 36.0f,  9.0f, 0.48f, 0.48f, 0.00f,  0.25f};
        case Expression::Asleep:    return {36.0f,  3.0f,  1.0f, 0.00f, 0.00f, 0.00f,  0.30f};
        case Expression::Angry:     return {36.0f, 32.0f,  6.0f, 0.50f, 0.05f, 0.00f,  0.10f};
        case Expression::Sad:       return {34.0f, 32.0f,  9.0f, 0.05f, 0.45f, 0.00f,  0.30f};
        case Expression::Surprised: return {42.0f, 44.0f, 15.0f, 0.00f, 0.00f, 0.00f, -0.10f};
        case Expression::Dizzy:     return {30.0f, 30.0f, 15.0f, 0.00f, 0.00f, 0.00f,  0.00f};
    }
    return {36.0f, 36.0f, 9.0f, 0.0f, 0.0f, 0.0f, 0.0f};
}

bool FaceAnimator::canBlink(Expression expression) {
    return expression != Expression::Asleep && expression != Expression::Dizzy;
}

FacePose FaceAnimator::update(float dtSeconds, Expression expression, std::optional<Gaze> gaze) {
    const float dt = std::clamp(dtSeconds, 0.0f, 0.1f);
    time_ += dt;

    const Shape target = shapeFor(expression);
    const float k = smoothingFactor(dt, kShapeTau);
    shape_.width = approach(shape_.width, target.width, k);
    shape_.height = approach(shape_.height, target.height, k);
    shape_.radius = approach(shape_.radius, target.radius, k);
    shape_.lidInner = approach(shape_.lidInner, target.lidInner, k);
    shape_.lidOuter = approach(shape_.lidOuter, target.lidOuter, k);
    shape_.lidBottom = approach(shape_.lidBottom, target.lidBottom, k);
    shape_.gazeBiasY = approach(shape_.gazeBiasY, target.gazeBiasY, k);

    const float openness = updateBlink(expression);

    Gaze gazeTarget = gaze ? *gaze : updateIdleGaze();
    if (expression == Expression::Asleep) {
        gazeTarget = {};
    }
    gazeTarget.x = std::clamp(gazeTarget.x, -1.0f, 1.0f);
    gazeTarget.y = std::clamp(gazeTarget.y + shape_.gazeBiasY, -1.0f, 1.0f);

    const float gk = smoothingFactor(dt, gaze ? kTrackedGazeTau : kIdleGazeTau);
    gaze_.x = approach(gaze_.x, gazeTarget.x, gk);
    gaze_.y = approach(gaze_.y, gazeTarget.y, gk);

    Gaze leftGaze = gaze_;
    Gaze rightGaze = gaze_;
    if (expression == Expression::Dizzy) {
        const float angle = time_ * 2.0f * kPi * kDizzySpinHz;
        leftGaze = {0.55f * std::cos(angle), 0.45f * std::sin(angle)};
        rightGaze = {-leftGaze.x, -leftGaze.y};
    }

    FacePose pose;
    pose.left = makeEye(kLeftEyeX, leftGaze, 1.0f - kPerspective * gaze_.x, openness);
    pose.right = makeEye(kRightEyeX, rightGaze, 1.0f + kPerspective * gaze_.x, openness);

    if (expression == Expression::Sleepy || expression == Expression::Asleep) {
        const float bob = kBreathingAmplitude * std::sin(time_ * 2.0f * kPi / kBreathingPeriod);
        pose.left.centerY += bob;
        pose.right.centerY += bob;
    }
    return pose;
}

float FaceAnimator::randomRange(float low, float high) {
    return std::uniform_real_distribution<float>(low, high)(rng_);
}

float FaceAnimator::updateBlink(Expression expression) {
    if (blinkStartedAt_ < 0.0f) {
        if (canBlink(expression) && time_ >= nextBlinkAt_) {
            blinkStartedAt_ = time_;
        } else {
            return 1.0f;
        }
    }

    const float duration = expression == Expression::Sleepy ? kSleepyBlinkDuration : kBlinkDuration;
    const float progress = (time_ - blinkStartedAt_) / duration;
    if (progress >= 1.0f) {
        blinkStartedAt_ = -1.0f;
        if (!doubleBlinkPending_ && randomRange(0.0f, 1.0f) < kDoubleBlinkChance) {
            doubleBlinkPending_ = true;
            nextBlinkAt_ = time_ + kDoubleBlinkGap;
        } else {
            doubleBlinkPending_ = false;
            nextBlinkAt_ = time_ + randomRange(kMinBlinkInterval, kMaxBlinkInterval);
        }
        return 1.0f;
    }
    return std::max(0.05f, 1.0f - std::sin(progress * kPi));
}

Gaze FaceAnimator::updateIdleGaze() {
    if (time_ >= nextSaccadeAt_) {
        if (randomRange(0.0f, 1.0f) < 0.35f) {
            idleTarget_ = {};
        } else {
            idleTarget_ = {randomRange(-0.8f, 0.8f), randomRange(-0.6f, 0.6f)};
        }
        nextSaccadeAt_ = time_ + randomRange(0.8f, 3.5f);
    }
    return idleTarget_;
}

EyePose FaceAnimator::makeEye(float baseCenterX, const Gaze& gaze, float sizeScale, float openness) const {
    EyePose eye;
    eye.width = shape_.width * sizeScale;
    const float openHeight = shape_.height * sizeScale;
    eye.height = std::max(2.0f, openHeight * openness);

    const float halfWidth = eye.width / 2.0f;
    const float halfHeight = openHeight / 2.0f;
    eye.centerX = std::clamp(baseCenterX + gaze.x * kGazeRangeX, halfWidth, kScreenWidth - halfWidth);
    eye.centerY = std::clamp(kEyeY + gaze.y * kGazeRangeY, halfHeight, kScreenHeight - halfHeight);

    eye.radius = std::min({shape_.radius * sizeScale, eye.width / 2.0f, eye.height / 2.0f});
    eye.lidInner = shape_.lidInner;
    eye.lidOuter = shape_.lidOuter;
    eye.lidBottom = shape_.lidBottom;
    return eye;
}

void renderFace(gfx::FrameBuffer& frame, const FacePose& pose) {
    frame.clear();
    renderEye(frame, pose.left, true);
    renderEye(frame, pose.right, false);
}

}  // namespace smartface
