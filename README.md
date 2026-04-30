# Spider-Bot — Architecture C++ avec cinématique inverse et FSM

## Vue d'ensemble

Le projet **Spider-Bot** est un robot hexapode piloté en **C++**.
Il est organisé autour de deux sous-systèmes indépendants :

- **ESP32** : gestion de la manette **PS5 (DualSense)**, calcul du mouvement, cinématique inverse, démarche tripode et commande des **18 servomoteurs** ;
- **Raspberry Pi Zero 2 W** : gestion de la partie **audio-visuelle** (caméra, microphone, haut-parleur).

Dans la version actuelle du projet, ces deux sous-systèmes sont **séparés** :
- l'ESP32 gère uniquement le **mouvement** ;
- la Raspberry Pi gère uniquement l'**audio/vidéo** ;
- il n'y a pas encore de **communication fonctionnelle active** entre eux.

Ce README a pour objectif :
- d'expliquer l'architecture logicielle du Spider-Bot ;
- de décrire le fonctionnement des pattes ;
- de faire le lien entre le code et l'architecture en **FSM** ;
- de documenter la procédure de **build PlatformIO** ;
- de servir de base documentaire pour le développement et pour GitHub Copilot.

---

## Parcours rapide actuel

Pour gagner du temps, le projet se lit dans cet ordre :

1. **Tester la DualSense seule** : profil `esp32dev-bt-test`, déjà validé.
2. **Tester la DualSense + 6 servos** : même profil `esp32dev-bt-test`, fichier `esp32/main/bt_test_main.c`.
3. **Tester les PWM sans manette** : profil `esp32dev-hybrid`, entrée manette simulée.
4. **Revenir au robot complet** : `esp32/main/main.cpp` -> `RobotController` -> FSM/IK/servos.

Le chemin matériel le plus fiable aujourd'hui est donc :

```powershell
cd esp32
pio run -e esp32dev-bt-test
pio run -e esp32dev-bt-test -t upload
pio device monitor -b 115200
```

Le profil `esp32dev-ps5-hybrid` reste l'objectif pour le robot complet, mais il dépend d'un header Arduino `Bluepad32.h`. S'il n'est pas disponible, le firmware compile mais retombe en entrée simulée. Pour les tests terrain actuels, utiliser `esp32dev-bt-test`.

---

## Stack technique

| Élément | Choix retenu | Pourquoi |
|---|---|---|
| Microcontrôleur | **ESP32 WROOM-32** (DevKit V1) | Bluetooth Classic (obligatoire pour DualSense) |
| Manette | **PS5 DualSense** via Bluepad32 | Lib mature, callbacks propres |
| Framework | **ESP-IDF + Arduino-core** (template Bluepad32) | Code Arduino classique, intégration Bluepad32 native |
| IDE | **VSCode + PlatformIO** | Multi-fichiers, autocomplétion, build rapide |
| Lib servos | **ESP32Servo** (madhephaestus) | 18 servos en GPIO direct, pas de PCA9685 dans la BOM |
| Langage | **C++ (Arduino-style)** | Performance pour la cinématique temps réel |

> ⚠️ **Choix de l'ESP32** : seules les puces **ESP32 « original »** (WROOM-32 / WROOM-32E) supportent le Bluetooth Classic nécessaire à la DualSense. Les variantes **ESP32-S2 / S3 / C3 / C6 / H2 ne fonctionnent PAS** avec Bluepad32 + DualSense (BLE uniquement).

---

## Objectif du système de locomotion

Le Spider-Bot est un **robot hexapode**, donc il possède :

- **6 pattes**
- **3 articulations par patte**
- soit **18 servomoteurs** (MG996R) au total

Chaque patte possède 3 degrés de liberté :
- **coxa** : articulation de rotation horizontale de la patte ;
- **femur** : articulation principale de levage / déploiement ;
- **tibia** : articulation terminale permettant d'ajuster la hauteur et l'extension du pied.

Dimensions caractéristiques (extraites du modèle CAO Fusion 360) :
- L1 (coxa) = **46 mm**
- L2 (fémur) = **87 mm**
- L3 (tibia) = **126 mm**
- Portée maximale : **213 mm**

L'objectif du système de locomotion est de :
1. recevoir une consigne utilisateur (joysticks DualSense) ;
2. convertir cette consigne en position cible cartésienne pour chaque pied ;
3. calculer les angles articulaires nécessaires (cinématique inverse) ;
4. envoyer les commandes correspondantes aux servomoteurs.

---

## Structure du projet

```text
programmation-spiderbot/
├── esp32/
│   ├── platformio.ini
│   ├── sdkconfig.defaults
│   ├── CMakeLists.txt
│   ├── components/                 ← Bluepad32 + BTstack (sous-modules)
│   └── main/
│       ├── CMakeLists.txt
│       ├── bt_test_main.c           ← test court DualSense + 6 servos
│       ├── main.cpp
│       ├── robot_controller.hpp / .cpp
│       ├── robot_fsm.hpp / .cpp
│       ├── tripod_walk_fsm.hpp / .cpp
│       ├── leg_fsm.hpp / .cpp
│       ├── servo_controller.hpp / .cpp
│       ├── input_manager.hpp / .cpp
│       ├── bluepad_manager.hpp / .cpp
│       ├── leg_kinematics.hpp / .cpp
│       └── esp32_runtime_wiring.hpp / .cpp
│
├── raspberry_pi/
│   ├── main.cpp
│   ├── camera_manager.hpp / .cpp
│   └── audio_manager.hpp / .cpp
│
├── common/
│   └── protocol.hpp / .cpp
│
├── docs/
│   ├── hybrid_validation_protocol.md
│   ├── hybrid_validation_checklist.md
│   ├── ps5_controller_and_servo_wiring.md
│   └── uml/
│       ├── fsm_globale_spiderbot.puml
│       ├── fsm_globale_simplifiee.puml
│       ├── fsm_locomotion_tripod.puml
│       └── fsm_patte.puml
│
├── ARCHITECTURE.md
└── README.md
```

Le point d'entrée ESP32 est `esp32/main/main.cpp`, qui instancie `RobotController`.
La boucle de contrôle réelle se trouve dans `esp32/main/robot_controller.cpp`.
`bluepad_manager.cpp` fait le pont entre l'API Bluepad32 et `InputManager`.

> Le sous-dossier `main/` est imposé par ESP-IDF. Tout le code source y vit. Le profil `esp32dev-bt-test` compile seulement `bt_test_main.c`; les autres profils compilent le robot C++ autour de `main.cpp`.

---

## Organisation objet

L'architecture objet implémentée sur ESP32 suit la hiérarchie suivante :

```text
RobotController
├── BluePadManager          (pont Bluepad32 → trames manette)
├── InputManager            (vx, vy, ωz, boutons → consigne robot)
│   ├── LocomotionFsm
│   └── LegKinematics
├── RobotFSM                (FSM globale : INIT, IDLE, WALK, SAFE_STOP, ERROR…)
├── TripodWalkFSM           (FSM locomotion : A_SUPPORT, B_SUPPORT, PAUSE…)
│   └── LegFSM[6]           (FSM patte : SUPPORT, LIFT, SWING, PLACE…)
└── ServoController         (wrapper ESP32Servo → 18 GPIO)
```

Cette organisation correspond directement aux trois diagrammes FSM dans `docs/uml/` :
- `fsm_globale_spiderbot.puml` → `RobotFSM`
- `fsm_locomotion_tripod.puml` → `TripodWalkFSM`
- `fsm_patte.puml` → `LegFSM` (6 instances)

---

## Cinématique inverse

`LegKinematics` implémente les équations détaillées dans la doc `Cinématique pattes` du projet :

1. **θ1 (coxa)** dans le plan horizontal :
   ```
   θ1 = atan2(y, x)
   ```
2. **Projection** dans le plan vertical (r' = √(x²+y²) − L1).
3. **θ3 (tibia)** par loi des cosinus :
   ```
   D = (r'² + z² − L2² − L3²) / (2·L2·L3)
   θ3 = atan2(√(1−D²), D)
   ```
4. **θ2 (fémur)** :
   ```
   φ = atan2(z, r')
   ψ = atan2(L3·sin(θ3), L2 + L3·cos(θ3))
   θ2 = φ − ψ
   ```

**Condition de validité** : la cible est atteignable ssi `−1 ≤ D ≤ 1`.
La méthode `LegKinematics::inverse()` retourne un `bool valid` ; si `false`, la `LegFSM` part en `LEG_ERROR`, qui remonte à la `TripodWalkFSM` puis à la `RobotFSM`.

---

## Démarche tripode

Implémentée dans `TripodWalkFSM`. Les 6 pattes sont divisées en deux trépieds :

- **Tripode A** : avant gauche, milieu droit, arrière gauche
- **Tripode B** : avant droit, milieu gauche, arrière droit

Les deux trépieds alternent en opposition de phase (décalage T/2). Pendant qu'un trépied est en **support** (3 pieds au sol), l'autre est en **swing** (transfert vers l'avant).

Trajectoire du pied en swing :
```
x(t) = x0 + v·t
z(t) = z0 + h·sin(π·t / T)
```
où `h` = hauteur de levée (~30 mm) et `T` = durée du demi-cycle (~1 s).

---

## Sécurité implémentée (prioritaire)

- **Timeout manette** centralisé dans `RobotController` (500 ms).
- **Perte de signal** → passage en `SafeStop` + recentrage progressif.
- **Défaut servo** détecté sur échec d'application des commandes.
- **Cible IK impossible** détectée par `LegKinematics` et remontée vers la FSM globale.
- **Mode `ERROR`** verrouillé par `RobotFSM` avec arrêt sécurisé des sorties.
- **CENTERING au boot** : interpolation lente (~2 s) vers la position neutre pour éviter les pics de courant et les chocs mécaniques.

---

## Mapping servo → GPIO (ESP32 sans PCA9685)

Sans driver PWM externe, les 18 servos sont câblés directement sur les GPIO de l'ESP32 via la lib `ESP32Servo` (multiplexage logiciel des timers LEDC).

| Patte | Coxa | Fémur | Tibia |
|---|---|---|---|
| Avant gauche (0) | GPIO 13 | GPIO 14 | GPIO 15 |
| Milieu gauche (1) | GPIO 16 | GPIO 17 | GPIO 18 |
| Arrière gauche (2) | GPIO 19 | GPIO 21 | GPIO 22 |
| Avant droite (3) | GPIO 23 | GPIO 25 | GPIO 26 |
| Milieu droite (4) | GPIO 27 | GPIO 32 | GPIO 33 |
| Arrière droite (5) | GPIO 4 | GPIO 5 | GPIO 2 |

> ⚠️ **GPIO interdits** : 6–11 (flash SPI interne, destruction garantie), 34–39 (input only). GPIO 0, 12, 15 sont des strapping pins → idéalement à éviter ou à protéger par pull-down 10 kΩ.

> ⚠️ **Pull-down 10 kΩ** recommandé sur chaque ligne de signal servo (vers GND) pour éviter le sursaut au boot ESP32 (les GPIO oscillent ~100 ms avant que le firmware prenne le contrôle).

> ⚠️ **GND commun obligatoire** entre l'ESP32, les XL4016 et la masse des servos. Ne pas relier la sortie 6 V des XL4016 au VCC de l'ESP32.

Voir `docs/ps5_controller_and_servo_wiring.md` pour le câblage détaillé et l'appairage DualSense.

---

## Intégration ESP32 (état actuel)

Les points d'accroche matériels sont exposés :

- **BluePadManager** :
  - `publishFrame(const GamepadData&)` pour publier une trame manette depuis un callback Bluepad32.
  - `notifyDisconnected()` pour signaler une perte de manette.
  - `update()` consomme une trame fraîche à la fois et retourne `std::nullopt` sinon.

- **ServoController** :
  - `setPwmWriteCallback(...)` pour brancher l'écriture PWM réelle (LEDC via ESP32Servo).
  - `setPwmDetachCallback(...)` pour le stop/release matériel des sorties.

Un module de wiring runtime est disponible :

- `esp32/main/esp32_runtime_wiring.hpp / .cpp`
- `docs/hybrid_validation_protocol.md` (procédure de test hybride matériel)
- `docs/hybrid_validation_checklist.md` (checklist terrain 1 page)

Il est appelé automatiquement depuis `esp32/main/main.cpp` via `configureEsp32RuntimeWiring(controller)`.

### Macros de build optionnelles

- `SPIDERBOT_USE_BLUEPAD32_EXAMPLE` : active le pont Bluepad32 → `BluePadManager`.
- `SPIDERBOT_USE_LEDC_EXAMPLE` : active la sortie PWM LEDC directe vers `ServoController`.

### Mode hybride de validation

Le wiring par défaut active 6 servos pour la validation initiale :

- Tableau par défaut : `0, 1, 2, 9, 10, 11`
- Patte avant gauche complète (servos 0, 1, 2)
- Patte avant droite complète (servos 9, 10, 11)
- Ordre de test : coxa → fémur → tibia (avant gauche), puis coxa → fémur → tibia (avant droite)

> **Note** : la lib ESP32Servo gère 18 servos avec multiplexage des timers LEDC, mais introduit un léger jitter au repos. Pour un robot de production, un PCA9685 (driver PWM I²C 16 canaux) serait préférable. Sur ce prototype, on accepte le compromis pour rester dans le budget BOM.

---

## Build ESP32 avec PlatformIO

### Prérequis

1. **VSCode** installé.
2. **Extension PlatformIO IDE** depuis le marketplace (un clic, redémarrage de VSCode).
3. **Driver USB-série** pour l'ESP32 :
   - DevKit avec puce CP2102 → driver Silicon Labs CP210x
   - DevKit avec puce CH340 → driver WCH CH340

### Premier build (clone fresh)

```powershell
git clone --recursive https://github.com/Krakendur/programmation-spiderbot.git
cd programmation-spiderbot/esp32
```

> Le `--recursive` est obligatoire pour récupérer les composants Bluepad32 et BTstack en sous-modules.

Si tu as oublié `--recursive` au clone :

```powershell
git submodule update --init --recursive
```

Ouvre le dossier `esp32/` dans VSCode (`File > Open Folder`).
PlatformIO détecte le `platformio.ini` et télécharge la toolchain ESP-IDF (5–10 min la première fois).

### Profils de build

Les environnements utiles sont définis dans `platformio.ini` :

| Profil | Description | Macros |
|---|---|---|
| `esp32dev-sim` | Build de base (simulation, pas de hardware) | aucune |
| `esp32dev-bt-test` | DualSense réelle + 6 servos de validation, sans FSM robot | `SPIDERBOT_BT_TEST` via script |
| `esp32dev-hybrid` | Sortie PWM réelle, manette simulée | `SPIDERBOT_USE_LEDC_EXAMPLE` |
| `esp32dev-ps5-hybrid` | Cible robot complet DualSense + PWM, encore dépendante de `Bluepad32.h` Arduino | `SPIDERBOT_USE_LEDC_EXAMPLE` + `SPIDERBOT_USE_BLUEPAD32_EXAMPLE` |

### Commandes principales

Depuis le dossier `esp32/` :

```powershell
# Chemin terrain actuel: manette + 6 servos
pio run -e esp32dev-bt-test
pio run -e esp32dev-bt-test -t upload --upload-port COM3
pio device monitor -b 115200 -p COM3

# Builds robot/simulation
pio run -e esp32dev-sim
pio run -e esp32dev-hybrid
pio run -e esp32dev-ps5-hybrid
```

Sous Linux/macOS, le port ressemble à `/dev/ttyUSB0` ou `/dev/cu.usbserial-XXXX`.

### Logs attendus au démarrage

Avec le chemin terrain actuel `esp32dev-bt-test` :

```text
[BT] Spider-Bot Bluepad32 + 6 servos test demarre
[SERVO] 6 servos centres a 90 degres
[BT] Bluetooth pret
[BT] DualSense prete!
[BT] Origine sticks capturee: ...
[PAD] lx= ...
```

Avec les profils robot C++ autour de `main.cpp`, le log suivant indique que le backend Arduino Bluepad32 est actif :

Si la manette Bluepad32 est correctement activée :
```
[ESP32] Bluepad32 example backend active
```

Si tu vois :
```
[ESP32] Bluepad32 backend disabled (simulation input)
```
alors :
- soit le profil de build actif n'inclut pas la macro `SPIDERBOT_USE_BLUEPAD32_EXAMPLE`,
- soit `Bluepad32.h` n'est pas disponible dans l'environnement de build (vérifier que `components/bluepad32/` est bien initialisé).
Pour le banc actuel, ce message n'est pas attendu : utilise plutôt `esp32dev-bt-test`.

### Appairage DualSense

1. Mettre la manette en mode pairing : maintenir **PS + Share** jusqu'à ce que la barre lumineuse clignote rapidement.
2. Démarrer l'ESP32 avec le firmware `esp32dev-bt-test` flashé.
3. La manette se connecte automatiquement (LED bleue fixe).
4. Si l'appairage reste bloqué, effacer la flash puis reflasher :

```powershell
pio run -e esp32dev-bt-test -t erase
pio run -e esp32dev-bt-test -t upload
```

Voir `docs/ps5_controller_and_servo_wiring.md` pour la procédure complète.

---

## Conventions de code

Pour assurer la cohérence du projet (et aider GitHub Copilot à générer du code aligné) :

- **Boucle 50 Hz strictement non-bloquante** : jamais de `delay()` long dans `loop()`. Les temporisations passent par `millis()` et l'état des FSMs.
- **Toute fonction IK retourne un `bool valid`** (ou `std::optional<JointAngles>`).
- **Toute transition FSM avec timeout** doit logger sur `Serial` au moins en mode debug.
- **Aucune allocation dynamique** dans la boucle de contrôle (pas de `new`/`malloc`/`std::vector::push_back` à 50 Hz).
- **Pas d'exceptions C++** : ESP-IDF désactive les exceptions par défaut, on retourne des codes d'erreur ou des `std::optional`.
- **Headers `.hpp`** pour le C++ pur, `.h` réservé aux includes C compatibles.
- **Noms de fichiers** en `snake_case`, **classes** en `PascalCase`, **méthodes/variables** en `camelCase`.

---

## Rappels de séparation des responsabilités

- **ESP32** : locomotion uniquement (entrées manette, FSM, IK, servo).
- **Raspberry Pi** : audio-visuel uniquement (caméra, micro, lecture audio).
- La conversion finale `(x, y, z) → angles servo → PWM` reste dans `ServoController`.
- `LegKinematics` ne touche **jamais** au matériel : c'est du calcul pur.
- `BluePadManager` ne touche **jamais** aux servos : il publie des trames consommées par `InputManager`.

---

## Roadmap

- [x] Architecture objet et FSM hiérarchiques
- [x] Cinématique inverse + directe (validées numériquement)
- [x] Squelette des FSMs (Robot, TripodWalk, Leg)
- [x] Migration vers PlatformIO + template Bluepad32
- [x] Appairage DualSense + lecture des axes
- [ ] Validation DualSense + 6 servos sur banc
- [ ] Validation 18 servos (robot complet posé)
- [ ] Premier déplacement tripod gait sur sol plat
- [ ] Communication ESP32 ↔ Raspberry Pi (UART ou I²C)
- [ ] Démarches alternatives (wave gait, ripple gait)

---

## Ressources

- **Cinématique** : voir le rapport `Cinematique pattes.docx` du projet.
- **Bilan énergétique** : voir `Bilan_energetique.docx`.
- **BOM matérielle** : voir `BOM_SpiderBot.xlsx`.
- **Bluepad32** : https://github.com/ricardoquesada/bluepad32
- **Template ESP-IDF + Arduino + Bluepad32** : https://github.com/ricardoquesada/esp-idf-arduino-bluepad32-template
- **ESP32Servo** : https://github.com/madhephaestus/ESP32Servo
