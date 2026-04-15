#pragma once

#include "bluepad_manager.hpp"
#include "input_manager.hpp"
#include "robot_fsm.hpp"
#include "servo_controller.hpp"
#include "tripod_walk_fsm.hpp"

#include <vector>

namespace spiderbot {

class RobotController {
public:
    explicit RobotController(const std::vector<int>& servoPins);

    // Starts the 50 Hz control loop.
    int run();

private:
    static long long nowMs();
    static const char* modeToString(RobotMode mode);

    static constexpr int kBluepadTimeoutMs = 500;

    ServoController servoController_;
    InputManager inputManager_;
    BluePadManager bluepad_;
    RobotFSM robotFsm_;
    TripodWalkFSM tripodWalkFsm_;

    long long gamepadLastActiveMs_;
    int iteration_;
    bool safeStopApplied_;
};

}  // namespace spiderbot
