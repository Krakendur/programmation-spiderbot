# Note de suivi des changements (agent)

Date: 2026-04-18
Branche: Spidey-c++

## Passe (deja effectue avant cette passe)

- Audit complet FSM/IK/entrees/sorties du repertoire Spider-Bot.
- Correction de la coherence entre `InputManager`, `LocomotionFsm` et `RobotFSM`.
- Propagation de l'etat d'erreur IK jusqu'a la FSM globale (`hasIkTargetError`).
- Reparation de `leg_kinematics.cpp`:
  - signature `solveLegIk(..., bool* unreachableTarget)` alignee,
  - tracking `hadUnreachableTarget` active,
  - correction du retour final (`return result`).
- Nettoyage de `esp32/main.cpp` pour conserver uniquement le point d'entree actif via `RobotController`.
- Mise a jour du README pour aligner l'architecture reelle (point d'entree, hierarchie objet, securite).

## Present (fait dans cette passe)

- BluepadManager rendu injectable par callbacks externes:
  - ajout `publishFrame(const GamepadData&)`,
  - ajout `notifyDisconnected()`,
  - ajout du mecanisme `hasFreshFrame_` pour consommer une trame fraiche par tick.
- ServoController rendu branchable hardware:
  - ajout `setPwmWriteCallback(...)`,
  - ajout `setPwmDetachCallback(...)`,
  - integration de ces callbacks dans `setServoPosition` et `stopAll`.
- RobotController ouvert a l'integration runtime:
  - ajout des acces `bluepadManager()` et `servoController()`,
  - ajout d'un `tickCallback` appele a chaque boucle.
- Ajout d'un module de wiring runtime ESP32:
  - `esp32/esp32_runtime_wiring.hpp`,
  - `esp32/esp32_runtime_wiring.cpp`.
- Activation du wiring depuis `esp32/main.cpp`:
  - appel `configureEsp32RuntimeWiring(controller)`.
- Ajout d'exemples optionnels (macro-gardes):
  - Bluepad32: `SPIDERBOT_USE_BLUEPAD32_EXAMPLE`,
  - LEDC: `SPIDERBOT_USE_LEDC_EXAMPLE`.
- README complete avec la section integration runtime ESP32 et limites LEDC.
- Documents utilisateur confirmes: pas de driver externe disponible pour le moment.
- Mode hybride de validation ajoute:
  - constante par defaut `kDefaultHybridValidationServoIds`;
  - 6 servos actifs par defaut,
  - patte avant gauche complete (0, 1, 2),
  - patte avant droite complete (9, 10, 11),
  - ordre pratique de test: coxa, femur, tibia sur chaque patte avant,
  - les autres servos restent en simulation logique.

## Futur (prochaines etapes recommandees)

- Valider le mode hybride sur banc, puis etendre progressivement le backend PWM direct sans driver externe.
- Valider la conversion des axes/boutons Bluepad32 selon la manette cible reelle.
- Ajouter une configuration centralisee des pins/channels/frequences PWM.
- Ajouter des tests materiels:
  - test de recentrage,
  - test perte de signal,
  - test defaut servo simule,
  - test cible IK inatteignable.
- Mettre a jour les diagrammes UML (`docs/uml`) pour reflecter exactement la FSM codee.
- Ajouter des instructions de build ESP32 (toolchain, macros, examples de flash) dans le README.
