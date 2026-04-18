#pragma once

namespace spiderbot {

enum class RobotMode {
    Idle,
    Teleop,
    SafeStop,
    Error,
};

struct RobotFsmInput {
    bool gamepadFrameReceived;
    bool gamepadSeen;
    bool signalLost;
    bool servoFault;
    bool ikError;
};

// Global supervisory FSM.
// It chooses the current mode and never performs IK computations.
class RobotFSM {
public:
    RobotFSM();

    void update(const RobotFsmInput& input);
    RobotMode mode() const;

private:
    RobotMode mode_;
};

}  // namespace spiderbot
