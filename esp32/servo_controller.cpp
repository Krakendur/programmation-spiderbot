#include "servo_controller.hpp"

#include <algorithm>
#include <iostream>

namespace spiderbot {

ServoController::ServoController(const std::vector<int>& servoPins) : servoPins_(servoPins) {
    // La position courante est conservee meme en mode simulation,
    // ce qui permet de tester la logique sans pilote PWM reel.
    currentPositions_.fill(90);
    std::cout << "[SERVO] Initialisation de " << servoPins_.size() << " pins" << std::endl;
}

bool ServoController::setServoPosition(int servoId, int angle) {
    if (servoId < 0 || servoId >= NUM_SERVOS) {
        return false;
    }

    const int clamped = std::clamp(angle, 0, 180);
    currentPositions_[servoId] = clamped;

    // Point d'integration materiel:
    // - ESP-IDF: ledc_set_duty/ledc_update_duty
    // - conversion angle -> pulse us selon vos servos
    return true;
}

bool ServoController::setAllPositions(const std::array<int, NUM_SERVOS>& angles) {
    for (int i = 0; i < NUM_SERVOS; ++i) {
        if (!setServoPosition(i, angles[i])) {
            return false;
        }
    }
    return true;
}

std::array<int, ServoController::NUM_SERVOS> ServoController::getAllPositions() const {
    return currentPositions_;
}

bool ServoController::centerAll() {
    std::array<int, NUM_SERVOS> centered{};
    centered.fill(90);
    return setAllPositions(centered);
}

void ServoController::stopAll() {
    // Point d'integration materiel:
    // - couper les canaux PWM
    // - ou forcer une position de securite
    std::cout << "[SERVO] Arret PWM" << std::endl;
}

}  // namespace spiderbot
