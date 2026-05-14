# Architecture Spider-Bot - etat actuel

## Vue d'ensemble

Spider-Bot est decoupe en deux sous-systemes volontairement separes:

- `esp32/`: locomotion, manette DualSense, FSM, cinematique inverse, servos et ecran ILI9488.
- `raspberry_pi/`: audio-visuel local, camera, micro et haut-parleur.

Aucun canal ESP32 <-> Raspberry Pi n'est actif dans cette version. Le dossier
`common/` contient seulement une base de protocole pour preparer cette evolution.

## Arborescence utile

```text
esp32/
  platformio.ini
  sdkconfig.defaults          # exceptions C++ activees, cible geree par PlatformIO
  CMakeLists.txt              # EXCLUDE_COMPONENTS=bluepad32 pour le build sim
  components/
    bluepad32/                # composant BT (exclu du build sim, utilise en hybrid)
    btstack/                  # stub CMakeLists.txt (sources absentes du depot)
    cmd_nvs/ cmd_system/      # utilitaires NVS pour Bluepad32
  main/
    main.cpp                  # app_main() ESP-IDF, appelle display.runMenuLoop()
    display_manager.hpp / .cpp
    menu_input.hpp / .cpp     # joystick ADC + boutons physiques (navigation menu)
    robot_controller.hpp / .cpp
    robot_fsm.hpp / .cpp
    tripod_walk_fsm.hpp / .cpp
    leg_fsm.hpp / .cpp
    locomotion_fsm.hpp / .cpp
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
Au demarrage, `DisplayManager` affiche un menu ILI9488 et selectionne l'application
(`Spiderbot` ou `Spiderkey`) via `runMenuLoop()`.

La chaine de demarrage est:

```text
app_main()
  -> DisplayManager::initialize()   SPI + ILI9488 init (landscape 480x320)
  -> MenuInput::initialize()         ADC oneshot (joystick) + GPIO pull-up (boutons)
  -> DisplayManager::runMenuLoop()  boucle joystick/boutons + clavier UART
  -> RobotController (mode selectionne)
       -> InputManager
       -> TripodWalkFSM + LegFSM[6]
       -> LegKinematics
       -> ServoController
       -> backend runtime (ESP32Servo ou simulation)
```

### DisplayManager - ecran ILI9488

- **Orientation** : landscape 480×320 (MADCTL 0x28, MV=1).
- **Couleurs** : RGB565 converties en 18-bit (3 octets) a l'envoi SPI.
- **Font** : Adafruit 5×7, rendue pixel par pixel, scalable (scale 1/2/3).
- **Menu** : 2 items (Spiderbot / Spiderkey), fond rouge sur la selection.
- **Navigation principale** : joystick analogique (axe Y) + deux boutons physiques.
  En parallele, le clavier UART (fleches ANSI ESC[A/B, Entree, Backspace) permet
  de naviguer depuis un terminal de debug sans materiel branche.
  Le timeout de 30 s retourne automatiquement sur Spiderbot.
- **Navigation phase 2** (a venir) : manette DualSense via Bluepad32.

### Pinout - controleur (branche Version-Spidercontroleur-PlatIO)

| Signal            | GPIO | Peripherique       | Remarques                          |
|-------------------|------|--------------------|------------------------------------|
| Joystick axe X    |  0   | ADC1_CH0           | axe horizontal (non utilise menu)  |
| Joystick axe Y    |  1   | ADC1_CH1           | axe vertical : haut/bas menu       |
| BTN1 - Valider    |  2   | GPIO entree        | pull-up interne, actif bas, 20 ms debounce |
| BTN2 - Retour     |  3   | GPIO entree        | pull-up interne, actif bas, 20 ms debounce |
| Ecran MOSI        |  5   | SPI2               | ILI9488                            |
| Ecran MISO        |  4   | SPI2               | ILI9488                            |
| Ecran CLK         |  6   | SPI2               | ILI9488                            |
| Ecran CS          | 19   | SPI2               | ILI9488                            |
| Ecran DC          | 11   | GPIO sortie        | Data/Command ILI9488               |
| Ecran RST         | 10   | GPIO sortie        | Reset ILI9488                      |
| Retro-eclairage   | 3.3V | (fixe)             | BL toujours allume                 |

Le joystick utilise l'**ADC oneshot** (unite ADC1, attenuation 12 dB, 12 bits).
La normalisation centre la valeur sur 0 avec une plage de −1.0 a +1.0.
Un seuil de declenchement a ±0.50 et une zone morte a ±0.05 evitent les faux
evenements. La machine d'etat joystick garantit une seule impulsion par inclinaison
(retour au centre obligatoire avant nouvel evenement).

Structure visuelle (calquee sur `terminal_menu.c`) :

```
┌─────────────── SPIDER-BOT ───────────────┐  header box (orange, scale=2)
├──────────────────────────────────────────┤
│  >  Spiderbot                            │  item selectionne (fond rouge, curseur ">")
│     Spiderkey                            │  item normal (gris)
├──────────────────────────────────────────┤
│  HAUT/BAS : naviguer  BTN1 : confirmer  BTN2 : retour  │  footer hints
└──────────────────────────────────────────┘
```

### FSM

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
pio run -e esp32dev-sim        # simulation : entree clavier UART, servos logiques
pio run -e esp32dev-hybrid     # hardware partiel : ESP32Servo sur 6 servos
pio run -e esp32dev-ps5-hybrid # hardware complet : Bluepad32 + ESP32Servo
```

Points cles du systeme de build:

- `framework = espidf` (pur ESP-IDF, pas Arduino).
- `EXCLUDE_COMPONENTS bluepad32` dans `CMakeLists.txt` : empeche la compilation
  de bluepad32 (dont les sources btstack sont incompletes) en mode sim.
- `CONFIG_COMPILER_CXX_EXCEPTIONS=y` dans `sdkconfig.defaults` : exceptions C++
  activees (remplacees par `assert` dans le code embarque, conservees cote Arduino).
- Les profils `hybrid` et `ps5-hybrid` definissent `SPIDERBOT_USE_LEDC_EXAMPLE`
  et/ou `SPIDERBOT_USE_BLUEPAD32_EXAMPLE` pour activer les backends hardware.

## Securites implementees

- Timeout manette centralise dans `RobotController` (`500 ms`).
- Perte de signal vers `SAFE_STOP`.
- Recentrement via `ServoController::centerAll()`.
- Remontee des erreurs IK impossibles jusqu'a `RobotFSM`.
- Remontee des defauts servo via le retour `false` des callbacks PWM.
- Mode `ERROR` verrouillant les sorties par `stopAll()`.
- Timeout menu ILI9488 : 30 s sans action → `Spiderbot` par defaut.
