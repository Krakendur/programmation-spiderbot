#include "robot_controller.hpp"

#include <cassert>
#include <iostream>
#include <thread>
#include <utility>

namespace spiderbot {

namespace {

#ifdef SPIDERBOT_TEST_SERVO_FAULT
constexpr bool kEnableServoFaultInjectionTest = true;
constexpr int kInjectedFaultServoId = -1;
#else
constexpr bool kEnableServoFaultInjectionTest = false;
constexpr int kInjectedFaultServoId = -1;
#endif

}  // namespace

RobotController::RobotController(const std::vector<int>& servoPins,
                                   RobotAppMode appMode,
                                   MenuInput* menuInput)
        : servoController_(servoPins),
          inputManager_(),
          bluepad_(),
          robotFsm_(),
          tripodWalkFsm_(),
          appMode_(appMode),
          menuInput_(menuInput),
          gamepadLastActiveMs_(spiderbot::nowMs()),
          iteration_(0),
          safeStopApplied_(false),
          lastLoggedMode_(RobotMode::Idle),
          tickCallback_() {
    assert(servoPins.size() == ServoController::NUM_SERVOS);
    std::cout << "[RobotController] App mode selected: "
              << (appMode_ == RobotAppMode::Spiderbot ? "Spiderbot" : "Spiderkey")
              << std::endl;
}

GamepadData RobotController::buildJoystickFrame() {
    GamepadData frame;
    frame.axes["lx"] = static_cast<double>(menuInput_->readAxisX());
    frame.axes["ly"] = static_cast<double>(menuInput_->readAxisY());
    frame.axes["rx"] = 0.0;
    frame.axes["ry"] = 0.0;
    frame.axes["lt"] = 0.0;
    frame.axes["rt"] = 0.0;
    frame.buttons["X"] = menuInput_->readBtnSelect();
    frame.buttons["B"] = menuInput_->readBtnBack();
    return frame;
}

BluePadManager& RobotController::bluepadManager() {
    return bluepad_;
}

ServoController& RobotController::servoController() {
    return servoController_;
}

void RobotController::setTickCallback(std::function<void()> callback) {
    tickCallback_ = std::move(callback);
}

const char* RobotController::modeToString(RobotMode mode) {
    switch (mode) {
        case RobotMode::Idle:
            return "IDLE";
        case RobotMode::Teleop:
            return "TELEOP";
        case RobotMode::SafeStop:
            return "SAFE_STOP";
        case RobotMode::Error:
            return "ERROR";
    }
    return "UNKNOWN";
}

int RobotController::run() {
    if (kEnableServoFaultInjectionTest) {
        servoController_.setFaultInjection(true, kInjectedFaultServoId);
        std::cout << "[TEST] Servo fault injection enabled" << std::endl;
    }

    bool servoFaultDetected = !servoController_.centerAll();

    while (true) {
        ++iteration_;
        const long long currentTime = spiderbot::nowMs();

        if (tickCallback_) {
            tickCallback_();
        }

        std::optional<GamepadData> gamepadData;
        if (appMode_ == RobotAppMode::Spiderkey && menuInput_) {
            gamepadData = buildJoystickFrame();
        } else {
            gamepadData = bluepad_.update();
        }

        const bool frameReceived = gamepadData.has_value();
        if (frameReceived) {
            gamepadLastActiveMs_ = currentTime;
            const bool tripodEnabled = tripodWalkFsm_.update(*gamepadData, currentTime);
            inputManager_.processBluepadInputWithTripod(*gamepadData, tripodEnabled);
            safeStopApplied_ = false;
        }

        // En mode Spiderkey, le joystick est toujours présent → pas de timeout.
        const bool gamepadSeen = (appMode_ == RobotAppMode::Spiderkey && menuInput_)
                                     ? true
                                     : inputManager_.isGamepadConnected();
        const bool signalLost = (appMode_ == RobotAppMode::Spiderkey && menuInput_)
                                    ? false
                                    : (gamepadSeen && (currentTime - gamepadLastActiveMs_ > kBluepadTimeoutMs));

        RobotFsmInput fsmInput{};
        fsmInput.gamepadFrameReceived = frameReceived;
        fsmInput.gamepadSeen = gamepadSeen;
        fsmInput.signalLost = signalLost;
        fsmInput.servoFault = servoFaultDetected;
        fsmInput.ikError = inputManager_.hasIkTargetError();
        robotFsm_.update(fsmInput);

        const RobotMode currentMode = robotFsm_.mode();
        if (currentMode != lastLoggedMode_ || iteration_ % 20 == 0) {
            std::cout << "[FSM] mode=" << modeToString(currentMode)
                      << " frame=" << (frameReceived ? "yes" : "no")
                      << " seen=" << (gamepadSeen ? "yes" : "no")
                      << " lost=" << (signalLost ? "yes" : "no")
                      << " ik=" << (fsmInput.ikError ? "yes" : "no")
                      << std::endl;
            lastLoggedMode_ = currentMode;
        }

        switch (currentMode) {
            case RobotMode::Teleop: {
                const auto commands = inputManager_.getServoCommands();
                if (!servoController_.setAllPositions(commands)) {
                    servoFaultDetected = true;
                }
                break;
            }
            case RobotMode::SafeStop:
                if (!safeStopApplied_) {
                    std::cout << "[BLUEPAD] Signal lost, applying safe stop" << std::endl;
                    inputManager_.reset();
                    if (!servoController_.centerAll()) {
                        servoFaultDetected = true;
                    }
                    safeStopApplied_ = true;
                }
                break;
            case RobotMode::Idle:
                // Idle mode keeps current posture.
                break;
            case RobotMode::Error:
                std::cout << "[SAFETY] Critical fault: "
                          << (inputManager_.hasIkTargetError() ? "IK target unreachable" : "servo fault")
                          << std::endl;
                inputManager_.reset();
                servoController_.centerAll();
                servoController_.stopAll();
                return 1;
        }

        if (iteration_ % 100 == 0) {
            if (appMode_ == RobotAppMode::Spiderkey && menuInput_) {
                std::cout << "[STATUS] loop=" << iteration_ << " joystick=CONNECTED"
                          << " mode=" << modeToString(currentMode) << std::endl;
            } else {
                std::cout << "[STATUS] loop=" << iteration_ << " bluepad="
                          << (bluepad_.isConnected() ? "CONNECTED" : "DISCONNECTED")
                          << " mode=" << modeToString(currentMode) << std::endl;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    return 0;
}

}  // namespace spiderbot
