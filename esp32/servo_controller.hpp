#pragma once

#include <array>
#include <vector>

namespace spiderbot {

// Abstraction de la couche servos.
// Cette classe centralise les 18 positions cibles et expose un point
// d'integration unique vers le driver PWM materiel.
class ServoController {
public:
    static constexpr int NUM_SERVOS = 18;

    // servoPins: mapping logique servo_id -> GPIO.
    explicit ServoController(const std::vector<int>& servoPins);

    // Positionne un servo unique en degres [0..180].
    // Retourne false si l'index est invalide.
    bool setServoPosition(int servoId, int angle);

    // Positionne tous les servos en une passe.
    // Le tableau d'entree doit respecter l'ordre des servo_id [0..17].
    bool setAllPositions(const std::array<int, NUM_SERVOS>& angles);

    // Retourne la derniere position connue de chaque servo.
    std::array<int, NUM_SERVOS> getAllPositions() const;

    // Ramene tous les servos a la position neutre (90 degres).
    bool centerAll();

    // Arret/relachement de la sortie PWM (selon implementation materielle).
    void stopAll();

    // Active une faute de sortie servo pour tester la chaine de securite.
    // failingServoId = -1 force l'echec de tous les servos.
    void setFaultInjection(bool enabled, int failingServoId = -1);

    // Indique si l'injection de faute est active.
    bool isFaultInjectionEnabled() const;

private:
    // Liste des GPIO associes aux 18 servos.
    std::vector<int> servoPins_;

    // Cache de l'etat courant en degres, utile en simulation et telemetrie.
    std::array<int, NUM_SERVOS> currentPositions_;

    // Outils de test securite: simulation de defaut servo.
    bool faultInjectionEnabled_;
    int failingServoId_;
};

}  // namespace spiderbot
