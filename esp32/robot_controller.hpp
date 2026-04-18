#pragma once

#include "bluepad_manager.hpp"
#include "control_constants.hpp"
#include "input_manager.hpp"
#include "robot_fsm.hpp"
#include "servo_controller.hpp"
#include "tripod_walk_fsm.hpp"

#include <functional>
#include <vector>

namespace spiderbot {

class RobotController {
public:
    explicit RobotController(const std::vector<int>& servoPins);

    // Starts the 50 Hz control loop.
    int run();

    // Expose des points d'integration pour la couche hardware.
    BluePadManager& bluepadManager();
    ServoController& servoController();

    // Callback optionnel appele a chaque tick de boucle avant la lecture des entrees.
    void setTickCallback(std::function<void()> callback);

private:
    static const char* modeToString(RobotMode mode);

    ServoController servoController_;
    InputManager inputManager_;
    BluePadManager bluepad_;
    RobotFSM robotFsm_;
    TripodWalkFSM tripodWalkFsm_;

    long long gamepadLastActiveMs_;
    int iteration_;
    bool safeStopApplied_;
    std::function<void()> tickCallback_;
};

}  // namespace spiderbot
