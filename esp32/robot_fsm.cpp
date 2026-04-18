#include "robot_fsm.hpp"

namespace spiderbot {

RobotFSM::RobotFSM() : mode_(RobotMode::Idle) {}

void RobotFSM::update(const RobotFsmInput& input) {
    if (mode_ == RobotMode::Error) {
        return;
    }

    if (input.servoFault || input.ikError) {
        mode_ = RobotMode::Error;
        return;
    }

    if (input.signalLost && input.gamepadSeen) {
        mode_ = RobotMode::SafeStop;
        return;
    }

    if (input.gamepadFrameReceived) {
        mode_ = RobotMode::Teleop;
        return;
    }

    mode_ = RobotMode::Idle;
}

RobotMode RobotFSM::mode() const {
    return mode_;
}

}  // namespace spiderbot
