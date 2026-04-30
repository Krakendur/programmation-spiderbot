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
    main.cpp
      display_manager.hpp / .cpp
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
Au demarrage, `DisplayManager` affiche un menu ILI9488 et selectionne l'application (`Spiderbot` ou `Spiderkey`).
La chaine de locomotion est:

```text
DisplayManager (menu ILI9488)
  -> BluePadManager
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

Le controle de l'écran ILI9488 et du menu de demarrage sont pris en charge par l'ESP32, sans modifier la separation actuelle entre audio-visuel et locomotion.

La Raspberry Pi ne pilote pas les servos dans l'etat actuel du projet.

## Build ESP32

Le build PlatformIO se lance depuis `esp32/`:

```powershell
pio run -e esp32dev-sim
pio run -e esp32dev-hybrid
pio run -e esp32dev-ps5-hybrid
```

Les fichiers `CMakeLists.txt` et `sdkconfig.defaults` gardent une base compatible
ESP-IDF + Arduino-core pour le template Bluepad32. Le profil PlatformIO actuel
reste le chemin le plus direct pour compiler le code du robot.

## Securites implementees

- Timeout manette centralise dans `RobotController` (`500 ms`).
- Perte de signal vers `SAFE_STOP`.
- Recentrement via `ServoController::centerAll()`.
- Remontee des erreurs IK impossibles jusqu'a `RobotFSM`.
- Remontee des defauts servo via le retour `false` des callbacks PWM.
- Mode `ERROR` verrouillant les sorties par `stopAll()`.
