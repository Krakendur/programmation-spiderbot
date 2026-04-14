#include "bluepad_manager.hpp"
#include "input_manager.hpp"
#include "servo_controller.hpp"

#include <array>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

namespace {

// Delai max sans trame manette avant de considerer le lien perdu.
constexpr int BLUEPAD_TIMEOUT_MS = 500;
// Strategie de securite: si la radio tombe, passer la FSM en Idle.
constexpr bool RF_LOST_AUTO_CENTER = true;

long long nowMs() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

std::vector<int> getServoPins() {
    // Mapping logique servo_id -> GPIO. A adapter au cablage reel.
    return {2, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32};
}

}  // namespace

int main() {
    const auto servoPins = getServoPins();
    if (servoPins.size() != 18) {
        std::cerr << "[ERREUR] Il faut exactement 18 GPIO pour les servos" << std::endl;
        return 1;
    }

    std::cout << "Spider-Bot ESP32 C++ - Initialisation" << std::endl;

    // Initialisation des trois briques principales du controle ESP32.
    spiderbot::ServoController servoController(servoPins);
    spiderbot::InputManager inputManager;
    spiderbot::BluePadManager bluepad;

    // Position mecanique sure au demarrage.
    servoController.centerAll();

    long long gamepadLastActive = nowMs();
    int iteration = 0;

    while (true) {
        ++iteration;
        const long long currentTime = nowMs();

        // 1) Lecture manette et conversion en consignes cartesianes/IK.
        const auto gamepadData = bluepad.update();
        if (gamepadData.has_value()) {
            gamepadLastActive = currentTime;
            inputManager.processBluepadInput(*gamepadData);
        } else if (currentTime - gamepadLastActive > BLUEPAD_TIMEOUT_MS) {
            if (inputManager.isGamepadConnected() && RF_LOST_AUTO_CENTER) {
                std::cout << "[BLUEPAD] Signal perdu, passage en Idle" << std::endl;
                inputManager.notifyGamepadDisconnected();
            }
        }

        // 2) Application des 18 angles vers la couche servos (PWM a brancher).
        const auto commands = inputManager.getServoCommands();
        servoController.setAllPositions(commands);

        // 3) Telemetrie periodique pour debug serie.
        if (iteration % 100 == 0) {
            std::cout << "[STATUS] boucle=" << iteration
                      << " bluepad=" << (inputManager.isGamepadConnected() ? "CONNECTE" : "DECONNECTE")
                      << " fsm=" << inputManager.getLocomotionStateName() << std::endl;
        }

        // 4) Boucle de controle a 50 Hz.
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    return 0;
}
