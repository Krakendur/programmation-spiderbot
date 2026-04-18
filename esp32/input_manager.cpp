#include "input_manager.hpp"

#include <algorithm>
#include <cmath>

namespace spiderbot {

// ============================================================
// InputManager: pont entre la manette et la cinematique.
//
// Glossaire rapide:
// - IK: Inverse Kinematics (cinematique inverse).
// - UART: Universal Asynchronous Receiver Transmitter.
// - lx/ly/rx/ry: axes sticks gauche (L) et droit (R), x/y.
// - lt/rt: triggers gauche (L) et droit (R).
//
// Pipeline de ce module:
// 1) Recevoir un etat manette (axes + boutons).
// 2) Le normaliser en commandes haut niveau (forward/lateral/yaw/height/lift).
// 3) Choisir le mode de production des angles:
//    - statique (posture stable) si activite faible,
//    - marche tripod si activite suffisante.
// 4) Obtenir un tableau de 18 angles depuis LegKinematics.
// 5) Exposer ce tableau au controleur de servos.
// ============================================================

InputManager::InputManager() : gamepadConnected_(false) {
    // Etat neutre: tous les servos au milieu.
    movementState_.fill(90);

    // Initialisation defensive: toutes les entrees de manette a zero.
    // Cela evite d envoyer un mouvement parasite avant la premiere vraie trame.
    gamepadState_.buttons = {};
    gamepadState_.axes = {
        {"lx", 0.0}, {"ly", 0.0}, {"rx", 0.0},
        {"ry", 0.0}, {"lt", 0.0}, {"rt", 0.0},
    };
    ikTargetError_ = false;
}

bool InputManager::processBluepadInput(const GamepadData& gamepadData) {
    // Entree principale en temps reel:
    // on memorise la derniere trame Bluepad puis on recalcule immediatement
    // les consignes servo pour garder une reponse fluide.
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
    // Objectif: robustesse face a une trame incomplete ou un changement de mapping.
    // Les lambdas evitent les acces directs risques a une map potentiellement incomplète.
    
    // Lambda pour extraire une valeur d'axe analogique (stick ou trigger).
    // Retourne 0.0 (position neutre) si la clé n'existe pas dans gamepadState_.axes.
    // Cela permet une reponse stable meme si le parsing Bluepad est defaillant.
    const auto getAxis = [&](const std::string& name) {
        const auto it = gamepadState_.axes.find(name);
        return (it == gamepadState_.axes.end()) ? 0.0 : it->second;
    };

    // Lambda pour extraire une valeur booléenne (bouton appuyé ou pas).
    // Retourne false (bouton non enfonce) si la clé n'existe pas.
    // Garantit une evaluation coherente meme en cas de trame Bluepad partielle.
    const auto getButton = [&](const std::string& name) {
        const auto it = gamepadState_.buttons.find(name);
        return (it != gamepadState_.buttons.end()) ? it->second : false;
    };

    // Extraction des axes analogiques:
    // - lx/ly: stick gauche (left), x (horizontal) et y (vertical)
    // - rx/ry: stick droit (right), x (horizontal) et y (vertical)
    // - lt/rt: triggers gauche (left) et droit (right), actionnent la levage des pattes
    const double lx = getAxis("lx");
    const double ly = getAxis("ly");
    const double rx = getAxis("rx");
    const double ry = getAxis("ry");
    const double lt = getAxis("lt");
    const double rt = getAxis("rt");

    // ------------------------------------------------------------
    // Mapping manette -> commandes haut niveau du robot
    // ------------------------------------------------------------
    // Convention de signes choisie pour un pilotage intuitif:
    // - ly vers le haut doit faire avancer le robot => forward = -ly
    // - ry vers le haut doit relever le corps      => height  = -ry
    // - lx pilote le deplacement lateral
    // - rx pilote le yaw (rotation gauche/droite autour de l axe vertical)
    //
    // lift provient des triggers:
    // - rt augmente la phase de levage des pattes
    // - lt la diminue
    // La formule ramene la difference [rt-lt] vers [0..1] avec clamp de securite.
    const double forward = -ly;
    const double lateral = lx;
    const double yaw = rx;
    const double height = -ry;
    const double lift = std::clamp((rt - lt + 1.0) * 0.5, 0.0, 1.0);
    const double activity = std::abs(forward) + std::abs(lateral) + std::abs(yaw);
    const bool tripodEnabled = useAutoTripodSelection ? (activity > kActivityThreshold) : forceTripodEnabled;

    // Raccourci operateur: recentrage instantane en posture neutre.
    if (getButton("X")) {
        movementState_.fill(90);
        ikTargetError_ = false;
        locomotionFsm_.reset();
        return;
    }

    LocomotionCommand command{};
    command.forward = forward;
    command.lateral = lateral;
    command.yaw = yaw;
    command.height = height;
    command.lift = lift;
    command.centerRequested = false;
    command.emergencyStopRequested = false;
    command.gamepadConnected = gamepadConnected_;
    command.tripodEnabled = tripodEnabled;
    command.nowMs = spiderbot::nowMs();

    movementState_ = locomotionFsm_.update(command, kinematics_);
    ikTargetError_ = locomotionFsm_.hadIkTargetError();
}

bool InputManager::processUartCommand(const std::string& commandJson) {
    // Canal secondaire: reception de commandes texte via UART.
    // Parser minimaliste volontaire pour la version actuelle.
    // Pour une version production, preferer un vrai parseur JSON
    // (ex: nlohmann::json) avec validation de schema.
    if (commandJson.find("\"type\":\"center_all\"") != std::string::npos) {
        // Commande de securite: retour au neutre des 18 servos.
        movementState_.fill(90);
        ikTargetError_ = false;
        locomotionFsm_.reset();
        return true;
    }

    return false;
}

std::array<int, 18> InputManager::getServoCommands() const {
    // Sortie principale du module: snapshot des 18 consignes servo.
    return movementState_;
}

void InputManager::reset() {
    // Reset local du module (sans effet sur l etat de connexion RF).
    movementState_.fill(90);
    ikTargetError_ = false;
    locomotionFsm_.reset();
}

bool InputManager::isGamepadConnected() const {
    // Indique si au moins une trame manette valide a ete recue.
    return gamepadConnected_;
}

void InputManager::notifyGamepadDisconnected() {
    gamepadConnected_ = false;
    reset();
}

bool InputManager::hasIkTargetError() const {
    return ikTargetError_;
}

LocomotionState InputManager::getLocomotionState() const {
    return locomotionFsm_.state();
}

const char* InputManager::getLocomotionStateName() const {
    return locomotionFsm_.stateName();
}

}  // namespace spiderbot
