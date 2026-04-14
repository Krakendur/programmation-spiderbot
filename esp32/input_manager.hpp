#pragma once

#include "bluepad_manager.hpp"
#include "leg_kinematics.hpp"
#include "locomotion_fsm.hpp"

#include <array>
#include <string>

namespace spiderbot {

// Traduit les entrees haut niveau (manette/UART) en 18 consignes servos.
// Cette classe est responsable du mapping de pilotage et du choix
// posture statique vs demarche tripod.
class InputManager {
public:
    InputManager();

    // Met a jour l'etat manette puis recalcule les angles servos.
    bool processBluepadInput(const GamepadData& gamepadData);

    // Traite une commande serie au format JSON (ou pseudo-JSON).
    // Exemple supporte actuellement: center_all.
    bool processUartCommand(const std::string& commandJson);

    // Retourne les 18 angles cibles [0..180].
    std::array<int, 18> getServoCommands() const;

    // Reinitialise les consignes en position neutre.
    void reset();

    // Signale une perte de connexion manette (timeout radio, etc.).
    void notifyGamepadDisconnected();

    // Indique si une manette a deja fourni un etat valide.
    bool isGamepadConnected() const;

    // Etat courant de la FSM locomotion.
    LocomotionState getLocomotionState() const;
    const char* getLocomotionStateName() const;

private:
    // Horloge monotone en millisecondes pour la phase de marche.
    static long long nowMs();

    // Applique le mapping axes/boutons -> commandes cinematiques.
    void computeServoPositionsFromGamepad();

    // Etat cible des 18 servos.
    std::array<int, 18> movementState_;

    // Dernier etat manette recu, conserve pour lissage/continuite.
    GamepadData gamepadState_;

    // Vrai si la manette a deja ete vue active.
    bool gamepadConnected_;

    // Moteur de cinematique inverse des pattes.
    LegKinematics kinematics_;

    // Machine d'etats locomotion.
    LocomotionFsm locomotionFsm_;
};

}  // namespace spiderbot
