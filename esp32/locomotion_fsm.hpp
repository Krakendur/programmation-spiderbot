#pragma once

#include "leg_kinematics.hpp"

#include <array>

namespace spiderbot {

enum class LocomotionState {
    Idle,
    Stand,
    Walk,
    Turn,
    EmergencyStop,
};

struct LocomotionCommand {
    double forward;
    double lateral;
    double yaw;
    double height;
    double lift;
    bool centerRequested;
    bool emergencyStopRequested;
    bool gamepadConnected;
    long long nowMs;
};

class LocomotionFsm {
public:
    LocomotionFsm();

    std::array<int, LegKinematics::NUM_SERVOS> update(const LocomotionCommand& command,
                                                       const LegKinematics& kinematics);

    void reset();
    LocomotionState state() const;
    const char* stateName() const;

private:
    static constexpr double kActivityThreshold = 0.12;
    static constexpr double kTurnThreshold = 0.20;

    static double absSum(double a, double b, double c);
    static const char* toString(LocomotionState state);

    LocomotionState state_;
};

}  // namespace spiderbot
