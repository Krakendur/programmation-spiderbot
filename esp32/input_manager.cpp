#include "input_manager.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace spiderbot {

InputManager::InputManager() : gamepadConnected_(false), ikTargetError_(false) {
    // Etat neutre: tous les servos au milieu.
    movementState_.fill(90);

    gamepadState_.buttons = {};
    gamepadState_.axes = {
        {"lx", 0.0}, {"ly", 0.0}, {"rx", 0.0},
        {"ry", 0.0}, {"lt", 0.0}, {"rt", 0.0},
    };
}

long long InputManager::nowMs() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

bool InputManager::processBluepadInput(const GamepadData& gamepadData) {
    // Memorise le dernier etat valide de la manette.
    gamepadState_ = gamepadData;
    gamepadConnected_ = true;
    computeServoPositionsFromGamepad(false, true);
    return true;
}

bool InputManager::processBluepadInputWithTripod(const GamepadData& gamepadData,
                                                 bool tripodEnabled) {
    gamepadState_ = gamepadData;
    gamepadConnected_ = true;
    computeServoPositionsFromGamepad(tripodEnabled, false);
    return true;
}

void InputManager::computeServoPositionsFromGamepad(bool forceTripodEnabled,
                                                    bool useAutoTripodSelection) {
    // Acces securise aux axes/boutons: fallback neutre si cle absente.
    const auto getAxis = [&](const std::string& name) {
        const auto it = gamepadState_.axes.find(name);
        return (it == gamepadState_.axes.end()) ? 0.0 : it->second;
    };

    const auto getButton = [&](const std::string& name) {
        const auto it = gamepadState_.buttons.find(name);
        return (it != gamepadState_.buttons.end()) ? it->second : false;
    };

    const double lx = getAxis("lx");
    const double ly = getAxis("ly");
    const double rx = getAxis("rx");
    const double ry = getAxis("ry");
    const double lt = getAxis("lt");
    const double rt = getAxis("rt");

    // Convention pilotage:
    // - Y gauche vers le haut = avance (signe inverse)
    // - Y droite vers le haut = corps plus haut (signe inverse)
    const double forward = -ly;
    const double lateral = lx;
    const double yaw = rx;
    const double height = -ry;
    const double lift = std::clamp((rt - lt + 1.0) * 0.5, 0.0, 1.0);

    // Raccourci operateur: recentrage instantane en posture neutre.
    if (getButton("X")) {
        movementState_.fill(90);
        ikTargetError_ = false;
        return;
    }

    // En dessous du seuil, on garde une posture statique stable.
    // Au dessus, on active la marche tripod cadencee par le temps.
    const double activity = std::abs(forward) + std::abs(lateral) + std::abs(yaw);
    const bool tripodEnabled =
        useAutoTripodSelection ? (activity > 0.12) : forceTripodEnabled;
    if (tripodEnabled) {
        const auto ik =
            kinematics_.computeTripodServoAnglesWithStatus(forward, lateral, yaw, height, nowMs(), lift);
        movementState_ = ik.angles;
        ikTargetError_ = ik.hadUnreachableTarget;
    } else {
        const auto ik = kinematics_.computeServoAnglesWithStatus(forward, lateral, yaw, height, lift);
        movementState_ = ik.angles;
        ikTargetError_ = ik.hadUnreachableTarget;
    }
}

bool InputManager::processUartCommand(const std::string& commandJson) {
    // Parser JSON simplifie. A remplacer par nlohmann::json ou equivalent si besoin.
    if (commandJson.find("\"type\":\"center_all\"") != std::string::npos) {
        movementState_.fill(90);
        return true;
    }

    return false;
}

std::array<int, 18> InputManager::getServoCommands() const {
    return movementState_;
}

void InputManager::reset() {
    movementState_.fill(90);
    ikTargetError_ = false;
}

bool InputManager::isGamepadConnected() const {
    return gamepadConnected_;
}

bool InputManager::hasIkTargetError() const {
    return ikTargetError_;
}

}  // namespace spiderbot
