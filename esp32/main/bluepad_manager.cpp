
#include "bluepad_manager.hpp"

#include <algorithm>
#include <array>
#include <iostream>

namespace spiderbot {

namespace {

constexpr std::array<const char*, 4> kJoystickAxes{{"lx", "ly", "rx", "ry"}};

double axisOrZero(const GamepadData& frame, const char* axisName) {
    const auto it = frame.axes.find(axisName);
    return (it == frame.axes.end()) ? 0.0 : it->second;
}

}  // namespace

BluePadManager::BluePadManager()
    : connected_(false), joystickOriginCaptured_(false), hasFreshFrame_(false) {
    // Ce module est volontairement minimal:
    // il isole la source de donnees manette du reste du controle robot.
    std::cout << "[BLUEPAD] Manager pret (publier des trames via publishFrame)" << std::endl;
}

std::optional<GamepadData> BluePadManager::update() {
    // Fonctionnement event-driven:
    // - sans nouvelle trame, on retourne std::nullopt
    // - quand publishFrame() est appelee, update() consomme exactement une trame
    if (!hasFreshFrame_ || !gamepadData_.has_value()) {
        return std::nullopt;
    }

    hasFreshFrame_ = false;
    connected_ = true;
    return gamepadData_;
}

void BluePadManager::publishFrame(const GamepadData& frame) {
    gamepadData_ = applyJoystickOrigin(frame);
    connected_ = true;
    hasFreshFrame_ = true;
}

void BluePadManager::notifyDisconnected() {
    connected_ = false;
    hasFreshFrame_ = false;
    joystickOriginCaptured_ = false;
    joystickOrigin_.clear();
    gamepadData_.reset();
}

bool BluePadManager::isConnected() const {
    return connected_;
}

std::optional<GamepadData> BluePadManager::getGamepadData() const {
    return gamepadData_;
}

GamepadData BluePadManager::applyJoystickOrigin(const GamepadData& frame) {
    if (!joystickOriginCaptured_) {
        for (const char* axisName : kJoystickAxes) {
            joystickOrigin_[axisName] = axisOrZero(frame, axisName);
        }
        joystickOriginCaptured_ = true;
        std::cout << "[BLUEPAD] Origine sticks capturee: "
                  << "lx=" << joystickOrigin_["lx"]
                  << " ly=" << joystickOrigin_["ly"]
                  << " rx=" << joystickOrigin_["rx"]
                  << " ry=" << joystickOrigin_["ry"] << std::endl;
    }

    GamepadData corrected = frame;
    for (const char* axisName : kJoystickAxes) {
        corrected.axes[axisName] = std::clamp(axisOrZero(frame, axisName) - joystickOrigin_[axisName],
                                              -1.0, 1.0);
    }
    return corrected;
}

}  // namespace spiderbot
