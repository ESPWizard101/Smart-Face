#include "Mood.hpp"

namespace smartface {

const char* toString(Expression expression) {
    switch (expression) {
        case Expression::Neutral:
            return "neutral";
        case Expression::Happy:
            return "happy";
        case Expression::Sleepy:
            return "sleepy";
        case Expression::Asleep:
            return "asleep";
        case Expression::Angry:
            return "angry";
        case Expression::Sad:
            return "sad";
        case Expression::Surprised:
            return "surprised";
        case Expression::Dizzy:
            return "dizzy";
    }
    return "unknown";
}

MoodEngine::MoodEngine(const Config& config, uint32_t seed) : config_(config), rng_(seed) {
    scheduleIdleHappy();
}

Expression MoodEngine::update(const MotionState& motion, uint32_t dtMs) {
    if (reaction_) {
        if (reaction_->remainingMs > dtMs) {
            reaction_->remainingMs -= dtMs;
        } else {
            reaction_ = followUp_;
            followUp_.reset();
        }
    }

    if (!motion.sensorOk) {
        dizzyMs_ = 0;
        upsideDownMs_ = 0;
        current_ = reaction_ ? reaction_->expression : Expression::Neutral;
        return current_;
    }

    if (motion.shake >= config_.dizzyShakeLevel) {
        dizzyMs_ += dtMs;
        reaction_.reset();
        followUp_.reset();
        current_ = Expression::Dizzy;
        return current_;
    }
    if (current_ == Expression::Dizzy) {
        if (dizzyMs_ >= config_.minDizzyForAngerMs) {
            react(Expression::Angry, config_.angryAfterShakeMs);
        }
        dizzyMs_ = 0;
    }

    if (motion.upsideDown) {
        upsideDownMs_ += dtMs;
        current_ = upsideDownMs_ < config_.surprisedWhenFlippedMs ? Expression::Surprised : Expression::Sad;
        return current_;
    }
    upsideDownMs_ = 0;

    const bool sleeping = current_ == Expression::Sleepy || current_ == Expression::Asleep;
    if (sleeping && motion.stillMs < config_.sleepyAfterStillMs) {
        react(Expression::Surprised, config_.wakeSurpriseMs, Reaction{Expression::Happy, config_.wakeHappyMs});
    }

    if (reaction_) {
        current_ = reaction_->expression;
        return current_;
    }

    if (motion.stillMs >= config_.asleepAfterStillMs) {
        current_ = Expression::Asleep;
        return current_;
    }
    if (motion.stillMs >= config_.sleepyAfterStillMs) {
        current_ = Expression::Sleepy;
        return current_;
    }

    if (untilIdleHappyMs_ > dtMs) {
        untilIdleHappyMs_ -= dtMs;
    } else {
        react(Expression::Happy, config_.idleHappyDurationMs);
        scheduleIdleHappy();
        current_ = Expression::Happy;
        return current_;
    }

    current_ = Expression::Neutral;
    return current_;
}

void MoodEngine::react(Expression expression, uint32_t durationMs, std::optional<Reaction> followUp) {
    reaction_ = Reaction{expression, durationMs};
    followUp_ = followUp;
}

void MoodEngine::scheduleIdleHappy() {
    std::uniform_int_distribution<uint32_t> interval(config_.idleHappyMinIntervalMs, config_.idleHappyMaxIntervalMs);
    untilIdleHappyMs_ = interval(rng_);
}

}  // namespace smartface
