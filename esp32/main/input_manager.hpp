#pragma once

#include "bluepad_manager.hpp"
#include "control_constants.hpp"
#include "leg_kinematics.hpp"
#include "locomotion_fsm.hpp"
#include "time_utils.hpp"

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

    // Variante explicite pilotee par la FSM locomotion.
    // Si tripodEnabled vaut false, la posture statique est forcee.
    bool processBluepadInputWithTripod(const GamepadData& gamepadData, bool tripodEnabled);

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

    // Signale si la derniere cible IK etait hors zone atteignable.
    bool hasIkTargetError() const;

    // Etat courant de la FSM locomotion.
    LocomotionState getLocomotionState() const;
    const char* getLocomotionStateName() const;

private:
    // Applique le mapping axes/boutons -> commandes cinematiques.
    // - useAutoTripodSelection=true: choix tripod selon activite joystick.
    // - useAutoTripodSelection=false: utilise forceTripodEnabled.
    void computeServoPositionsFromGamepad(bool forceTripodEnabled,
                                          bool useAutoTripodSelection);

    // Etat cible des 18 servos.
    std::array<int, 18> movementState_;

    // Dernier etat manette recu, conserve pour lissage/continuite.
    GamepadData gamepadState_;

    // Vrai si la manette a deja ete vue active.
    bool gamepadConnected_;

    // Memorise un depassement IK sur le dernier calcul.
    bool ikTargetError_;

    // Moteur de cinematique inverse des pattes.
    LegKinematics kinematics_;

    // Machine d'etats locomotion.
    LocomotionFsm locomotionFsm_;
};

}  // namespace spiderbot
