#include "bluepad_manager.hpp"

#include <iostream>

namespace spiderbot {

BluePadManager::BluePadManager() : connected_(false), hasFreshFrame_(false) {
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
    gamepadData_ = frame;
    connected_ = true;
    hasFreshFrame_ = true;
}

void BluePadManager::notifyDisconnected() {
    connected_ = false;
    hasFreshFrame_ = false;
    gamepadData_.reset();
}

bool BluePadManager::isConnected() const {
    return connected_;
}

std::optional<GamepadData> BluePadManager::getGamepadData() const {
    return gamepadData_;
}

}  // namespace spiderbot
