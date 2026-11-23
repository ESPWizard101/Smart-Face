#pragma once

#include <cstdint>
#include <optional>
#include <random>

#include "MotionState.hpp"

namespace smartface {

enum class Expression : uint8_t {
    Neutral,
    Happy,
    Sleepy,
    Asleep,
    Angry,
    Sad,
    Surprised,
    Dizzy,
};

const char* toString(Expression expression);

class MoodEngine {
public:
    struct Config {
        float dizzyShakeLevel = 0.5f;
        uint32_t minDizzyForAngerMs = 400;
        uint32_t angryAfterShakeMs = 2500;
        uint32_t surprisedWhenFlippedMs = 1500;
        uint32_t sleepyAfterStillMs = 45000;
        uint32_t asleepAfterStillMs = 120000;
        uint32_t wakeSurpriseMs = 700;
        uint32_t wakeHappyMs = 2000;
        uint32_t idleHappyMinIntervalMs = 20000;
        uint32_t idleHappyMaxIntervalMs = 45000;
        uint32_t idleHappyDurationMs = 1500;
    };

    explicit MoodEngine(uint32_t seed) : MoodEngine(Config{}, seed) {}
    MoodEngine(const Config& config, uint32_t seed);

    Expression update(const MotionState& motion, uint32_t dtMs);

    Expression current() const { return current_; }

private:
    struct Reaction {
        Expression expression;
        uint32_t remainingMs;
    };

    void react(Expression expression, uint32_t durationMs, std::optional<Reaction> followUp = std::nullopt);
    void scheduleIdleHappy();

    Config config_;
    std::minstd_rand rng_;
    Expression current_ = Expression::Neutral;

    std::optional<Reaction> reaction_;
    std::optional<Reaction> followUp_;
    uint32_t dizzyMs_ = 0;
    uint32_t upsideDownMs_ = 0;
    uint32_t untilIdleHappyMs_ = 0;
};

}  // namespace smartface
