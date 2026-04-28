#include "servo_controller.hpp"

#include <algorithm>
#include <iostream>

namespace spiderbot {

// -----------------------------------------------------------------------------
// Constructeur
// -----------------------------------------------------------------------------
// Objectif:
// - memoriser la correspondance servo_id -> GPIO
// - initialiser un etat coherent des 18 servos
//
// Important:
// - ici on ne touche pas encore au materiel PWM
// - on prepare juste l'etat interne pour que le reste du logiciel
//   (IK, InputManager, etc.) puisse fonctionner meme en simulation
// -----------------------------------------------------------------------------
ServoController::ServoController(const std::vector<int>& servoPins)
        : servoPins_(servoPins),
            faultInjectionEnabled_(false),
            failingServoId_(-1),
            pwmWriteCallback_(),
            pwmDetachCallback_() {
    // Position neutre logique: 90 degres pour chaque servo.
    // Cette valeur est volontairement "safe" pour la plupart des montages.
    currentPositions_.fill(90);

    // Trace utile au demarrage pour verifier qu'on a bien 18 sorties configurees.
    std::cout << "[SERVO] Initialisation de " << servoPins_.size() << " pins" << std::endl;
}

// -----------------------------------------------------------------------------
// setServoPosition
// -----------------------------------------------------------------------------
// Role:
// - mettre a jour un seul servo (index + angle)
//
// Contrats:
// - servoId doit etre dans [0, NUM_SERVOS-1]
// - angle est borne a [0, 180] pour eviter une commande hors plage
//
// Note integration materielle:
// - cette methode est le meilleur point pour ecrire le vrai signal PWM
// - c'est ici qu'on convertit angle -> impulsion (us) -> duty cycle
// -----------------------------------------------------------------------------
bool ServoController::setServoPosition(int servoId, int angle) {
    // Protection anti index invalide pour eviter ecriture hors tableau.
    if (servoId < 0 || servoId >= NUM_SERVOS) {
        return false;
    }

    if (static_cast<size_t>(servoId) >= servoPins_.size()) {
        return false;
    }

    if (faultInjectionEnabled_ && (failingServoId_ == -1 || failingServoId_ == servoId)) {
        return false;
    }

    // Borne logicielle de securite (par rapport à l'angle).
    const int clamped = std::clamp(angle, 0, 180);

    // Si un callback hardware est fourni, c'est lui qui pilote la sortie PWM.
    if (pwmWriteCallback_) {
        if (!pwmWriteCallback_(servoId, servoPins_[servoId], clamped)) {
            return false;
        }
    }

    currentPositions_[servoId] = clamped;

    // Point d'integration materiel:
    // - ESP-IDF: ledc_set_duty/ledc_update_duty
    // - conversion angle -> pulse us selon vos servos
    //   exemple classique MG996R (a calibrer):
    //   pulse_us = 1000 + (angle / 180.0) * (2000 - 1000)
    //   puis conversion pulse_us -> duty selon la frequence PWM (50 Hz)
    return true;
}

// -----------------------------------------------------------------------------
// setAllPositions
// -----------------------------------------------------------------------------
// Role:
// - appliquer les 18 commandes en sequence
//
// Pourquoi passer par setServoPosition:
// - on reutilise la meme validation (index, borne angle)
// - on garantit un comportement uniforme single-servo / all-servos
// -----------------------------------------------------------------------------
bool ServoController::setAllPositions(const std::array<int, NUM_SERVOS>& angles) {
    for (int i = 0; i < NUM_SERVOS; ++i) {
        if (!setServoPosition(i, angles[i])) {
            // Si un servo echoue, on remonte l'information immediatement.
            return false;
        }
    }
    return true;
}

// Retourne une copie des 18 angles cibles actuellement memorises.
std::array<int, ServoController::NUM_SERVOS> ServoController::getAllPositions() const {
    return currentPositions_;
}

// -----------------------------------------------------------------------------
// centerAll
// -----------------------------------------------------------------------------
// Utilitaire de securite / maintenance:
// - utile au boot
// - utile apres perte de manette
// - utile pour verifier rapidement le montage mecanique
// -----------------------------------------------------------------------------
bool ServoController::centerAll() {
    std::array<int, NUM_SERVOS> centered{};
    centered.fill(90);
    return setAllPositions(centered);
}

// -----------------------------------------------------------------------------
// stopAll
// -----------------------------------------------------------------------------
// Intention:
// - point d'entree unique pour desactiver ou mettre en mode securise la PWM
//
// Strategies possibles selon votre hardware:
// - couper totalement les canaux PWM
// - maintenir une position de parking
// - relacher le couple (attention a la gravite sur les pattes)
// -----------------------------------------------------------------------------
void ServoController::stopAll() {
    if (pwmDetachCallback_) {
        for (int i = 0; i < NUM_SERVOS && static_cast<size_t>(i) < servoPins_.size(); ++i) {
            pwmDetachCallback_(i, servoPins_[i]);
        }
    }
    std::cout << "[SERVO] Arret PWM" << std::endl;
}

void ServoController::setFaultInjection(bool enabled, int failingServoId) {
    faultInjectionEnabled_ = enabled;
    failingServoId_ = failingServoId;
}

bool ServoController::isFaultInjectionEnabled() const {
    return faultInjectionEnabled_;
}

void ServoController::setPwmWriteCallback(std::function<bool(int, int, int)> callback) {
    pwmWriteCallback_ = std::move(callback);
}

void ServoController::setPwmDetachCallback(std::function<void(int, int)> callback) {
    pwmDetachCallback_ = std::move(callback);
}

}  // namespace spiderbot
