#include "locomotion_fsm.hpp"

#include <algorithm>
#include <cmath>

namespace spiderbot {

LocomotionFsm::LocomotionFsm() : state_(LocomotionState::Idle), hadIkTargetError_(false) {}

double LocomotionFsm::absSum(double a, double b, double c) {
    return std::abs(a) + std::abs(b) + std::abs(c);
}

const char* LocomotionFsm::toString(LocomotionState state) {
    switch (state) {
        case LocomotionState::Idle:
            return "Idle";
        case LocomotionState::Stand:
            return "Stand";
        case LocomotionState::Walk:
            return "Walk";
        case LocomotionState::Turn:
            return "Turn";
        case LocomotionState::EmergencyStop:
            return "EmergencyStop";
        default:
            return "Unknown";
    }
}

std::array<int, LegKinematics::NUM_SERVOS> LocomotionFsm::update(const LocomotionCommand& command,
                                                                  const LegKinematics& kinematics) {
    hadIkTargetError_ = false;

    const double clampedForward = std::clamp(command.forward, -1.0, 1.0);
    const double clampedLateral = std::clamp(command.lateral, -1.0, 1.0);
    const double clampedYaw = std::clamp(command.yaw, -1.0, 1.0);
    const double clampedHeight = std::clamp(command.height, -1.0, 1.0);
    const double clampedLift = std::clamp(command.lift, 0.0, 1.0);

    if (command.emergencyStopRequested) {
        state_ = LocomotionState::EmergencyStop;
    } else if (!command.gamepadConnected) {
        state_ = LocomotionState::Idle;
    } else if (command.centerRequested) {
        state_ = LocomotionState::Stand;
    } else if (!command.tripodEnabled) {
        state_ = LocomotionState::Stand;
    } else {
        const double moveActivity = absSum(clampedForward, clampedLateral, 0.0);
        const double turnActivity = std::abs(clampedYaw);

        if (moveActivity > kActivityThreshold) {
            state_ = LocomotionState::Walk;
        } else if (turnActivity > kTurnThreshold) {
            state_ = LocomotionState::Turn;
        } else {
            state_ = LocomotionState::Stand;
        }
    }

    if (state_ == LocomotionState::Idle || state_ == LocomotionState::Stand ||
        state_ == LocomotionState::EmergencyStop) {
        std::array<int, LegKinematics::NUM_SERVOS> neutral{};
        neutral.fill(90);
        return neutral;
    }

    if (state_ == LocomotionState::Turn) {
        const auto result = kinematics.computeTripodServoAnglesWithStatus(0.0,
                                                                          0.0,
                                                                          clampedYaw,
                                                                          clampedHeight,
                                                                          command.nowMs,
                                                                          clampedLift);
        hadIkTargetError_ = result.hadUnreachableTarget;
        return result.angles;
    }

    const auto result = kinematics.computeTripodServoAnglesWithStatus(clampedForward,
                                                                      clampedLateral,
                                                                      clampedYaw,
                                                                      clampedHeight,
                                                                      command.nowMs,
                                                                      clampedLift);
    hadIkTargetError_ = result.hadUnreachableTarget;
    return result.angles;
}

void LocomotionFsm::reset() {
    state_ = LocomotionState::Idle;
    hadIkTargetError_ = false;
}

LocomotionState LocomotionFsm::state() const {
    return state_;
}

const char* LocomotionFsm::stateName() const {
    return toString(state_);
}

bool LocomotionFsm::hadIkTargetError() const {
    return hadIkTargetError_;
}

}  // namespace spiderbot
