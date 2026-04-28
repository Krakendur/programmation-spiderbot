# Note de suivi des changements (agent)

## Passe 2026-04-28 - alignement avec le nouveau README

- Reorganisation de l'arborescence ESP32:
  - sources de `esp32/*.cpp/.hpp` deplacees vers `esp32/main/`;
  - `platformio.ini` deplace vers `esp32/platformio.ini`;
  - composants Bluepad32/BTstack deplaces de `external/.../components` vers `esp32/components`;
  - suppression du dossier `external/` restant et du dossier `build/` versionne.
- Ajout des fichiers de structure declares dans le README:
  - `esp32/CMakeLists.txt`;
  - `esp32/main/CMakeLists.txt`;
  - `esp32/sdkconfig.defaults`;
  - `common/protocol.hpp`;
  - `common/protocol.cpp`.
- Mise a jour de `esp32/platformio.ini` pour compiler depuis `esp32/main/`.
- Alignement du mapping servo -> GPIO dans `esp32/main/main.cpp` avec le tableau README.
- Remplacement du backend PWM direct LEDC par un backend `ESP32Servo` garde par la macro existante `SPIDERBOT_USE_LEDC_EXAMPLE`.
- Mise a jour de `.gitignore`, `ARCHITECTURE.md` et `.github/copilot-instructions.md` pour refleter la nouvelle structure.

Points a verifier par une passe suivante:

- Le profil `esp32dev-sim` doit compiler sans materiel depuis `esp32/`.
- Les profils hardware installent `madhephaestus/ESP32Servo`; verifier sur machine avec acces PlatformIO.
- Le backend DualSense reste conditionne par la presence reelle de `Bluepad32.h`.

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
- Nettoyage de `esp32/main/main.cpp` pour conserver uniquement le point d'entree actif via `RobotController`.
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
  - `esp32/main/esp32_runtime_wiring.hpp`,
  - `esp32/main/esp32_runtime_wiring.cpp`.
- Activation du wiring depuis `esp32/main/main.cpp`:
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

## Validation de cette passe

- Compilation desktop de la simulation ESP32 reussie avec `g++`.
- Lancement de `spiderbot_esp32_sim.exe` reussi sans materiel.
- Verification dans les logs de:
  - l'initialisation generale,
  - le mode simulation sans backend hardware,
  - les phases `REST`, `FORWARD`, `TURN`, `CENTER`, `WALK_LIGHT`,
  - les logs FSM `[FSM]` au moins au passage teleop,
  - les logs d'etat periodiques `[STATUS]`.
- Arret volontaire du processus apres confirmation du demarrage et du deroulement des phases.

## Ajustement apres validation

- Le log FSM a ete rendu periodique dans `RobotController` pour apparaitre meme quand le mode reste `TELEOP`.
- Objectif: garder une trace visible de l'activite FSM pendant la simulation complete sans materiel.

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
