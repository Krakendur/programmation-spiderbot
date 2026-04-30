# Architecture Spider-Bot - etat actuel

## Vue d'ensemble

Spider-Bot est decoupe en deux sous-systemes volontairement separes:

- `esp32/`: locomotion, manette DualSense, FSM, cinematique inverse et servos.
- `raspberry_pi/`: audio-visuel local, camera, micro et haut-parleur.

Aucun canal ESP32 <-> Raspberry Pi n'est actif dans cette version. Le dossier
`common/` contient seulement une base de protocole pour preparer cette evolution.

## Arborescence utile

```text
esp32/
  platformio.ini
  sdkconfig.defaults
  CMakeLists.txt
  components/
  main/
    bt_test_main.c               # test court DualSense + 6 servos
    main.cpp
    robot_controller.hpp / .cpp
    robot_fsm.hpp / .cpp
    tripod_walk_fsm.hpp / .cpp
    leg_fsm.hpp / .cpp
    servo_controller.hpp / .cpp
    input_manager.hpp / .cpp
    bluepad_manager.hpp / .cpp
    leg_kinematics.hpp / .cpp
    esp32_runtime_wiring.hpp / .cpp

raspberry_pi/
  main.cpp
  camera_manager.hpp / .cpp
  audio_manager.hpp / .cpp

common/
  protocol.hpp / .cpp
```

## Responsabilites ESP32

`RobotController` orchestre la boucle 50 Hz et applique les priorites de securite.
La chaine de locomotion est:

```text
BluePadManager
  -> InputManager
  -> TripodWalkFSM + LegFSM[6]
  -> LegKinematics
  -> ServoController
  -> backend runtime ESP32Servo ou simulation
```

Les FSM sont separees par niveau:

- `RobotFSM`: mode global (`IDLE`, `TELEOP`, `SAFE_STOP`, `ERROR`).
- `TripodWalkFSM`: alternance des deux tripodes.
- `LegFSM`: etat local de chaque patte.

`LegKinematics` reste du calcul pur et ne touche jamais au materiel.
`ServoController` est le seul point de sortie vers les PWM.

## Responsabilites Raspberry Pi

Le code Raspberry Pi reste limite a l'audio-visuel:

- `camera_manager`: capture et gestion camera.
- `audio_manager`: entree micro et sortie audio.
- `main.cpp`: boucle locale de demonstration.

La Raspberry Pi ne pilote pas les servos dans l'etat actuel du projet.

## Build ESP32

Le build PlatformIO se lance depuis `esp32/`:

```powershell
pio run -e esp32dev-bt-test
pio run -e esp32dev-sim
pio run -e esp32dev-hybrid
pio run -e esp32dev-ps5-hybrid
```

Repere rapide:

- `esp32dev-bt-test`: chemin materiel court actuel, DualSense reelle + 6 servos, sans FSM robot.
- `esp32dev-sim`: robot C++ sans hardware.
- `esp32dev-hybrid`: robot C++ + PWM reelles, entree manette simulee.
- `esp32dev-ps5-hybrid`: cible robot complet DualSense + PWM, encore dependante d'un header Arduino `Bluepad32.h`.

Les fichiers `CMakeLists.txt` et `sdkconfig.defaults` gardent une base compatible
ESP-IDF + Arduino-core. Le test Bluepad32 fiable passe par ESP-IDF pur dans
`bt_test_main.c`; le robot complet C++ reste autour de `main.cpp`.

## Securites implementees

- Timeout manette centralise dans `RobotController` (`500 ms`).
- Perte de signal vers `SAFE_STOP`.
- Recentrement via `ServoController::centerAll()`.
- Remontee des erreurs IK impossibles jusqu'a `RobotFSM`.
- Remontee des defauts servo via le retour `false` des callbacks PWM.
- Mode `ERROR` verrouillant les sorties par `stopAll()`.
