#include "bluepad_manager.hpp"

#include <iostream>

namespace spiderbot {

BluePadManager::BluePadManager() : connected_(false) {
    // Ce module est volontairement minimal:
    // il isole la source de donnees manette du reste du controle robot.
    std::cout << "[BLUEPAD] Mode simulation active" << std::endl;
}

std::optional<GamepadData> BluePadManager::update() {
    // Stub: brancher ici la librairie manette cible (ESP-IDF, Bluepad32 C++, etc.).
    // Le contrat attendu est un GamepadData normalise (axes [-1..1], boutons booleens).
    // En mode stub, on publie explicitement l'absence de signal.
    connected_ = false;
    gamepadData_.reset();
    return std::nullopt;
}

bool BluePadManager::isConnected() const {
    return connected_;
}

std::optional<GamepadData> BluePadManager::getGamepadData() const {
    return gamepadData_;
}

}  // namespace spiderbot
