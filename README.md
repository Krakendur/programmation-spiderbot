# Spider-Bot - Architecture C++ avec cinématique inverse et FSM

## Vue d’ensemble

Le projet **Spider-Bot** est un robot hexapode piloté en **C++**.  
Il est organisé autour de deux sous-systèmes indépendants :

- **ESP32** : gestion de la manette, calcul du mouvement, cinématique inverse, démarche et commande des **18 servomoteurs** ;
- **Raspberry Pi Zero 2 W** : gestion de la partie **audio-visuelle** (caméra, microphone, haut-parleur).

Dans la version actuelle du projet, ces deux sous-systèmes sont **séparés** :
- l’ESP32 gère uniquement le **mouvement** ;
- la Raspberry Pi gère uniquement l’**audio/vidéo** ;
- il n’y a pas encore de **communication fonctionnelle active** entre eux.

Ce README a pour objectif :
- d’expliquer l’architecture logicielle du Spider-Bot ;
- de décrire le fonctionnement des pattes ;
- de faire le lien entre le code actuel et une future architecture en **FSM** vue en cours ;
- de servir de base documentaire pour le développement et pour GitHub Copilot.

---

## Objectif du système de locomotion

Le Spider-Bot est un **robot hexapode**, donc il possède :

- **6 pattes**
- **3 articulations par patte**
- soit **18 servomoteurs** au total

Chaque patte possède 3 degrés de liberté :
- **coxa** : articulation de rotation horizontale de la patte ;
- **femur** : articulation principale de levage / déploiement ;
- **tibia** : articulation terminale permettant d’ajuster la hauteur et l’extension du pied.

L’objectif du système de locomotion est de :
1. recevoir une consigne utilisateur ;
2. convertir cette consigne en une position cible pour chaque pied ;
3. calculer les angles articulaires nécessaires ;
4. envoyer les commandes correspondantes aux servomoteurs.

---

## Structure du projet

```text
programmation spiderbot/
├── esp32/
│   ├── main.cpp
│   ├── robot_controller.hpp
│   ├── robot_controller.cpp
│   ├── robot_fsm.hpp
│   ├── robot_fsm.cpp
│   ├── tripod_walk_fsm.hpp
│   ├── tripod_walk_fsm.cpp
│   ├── leg_fsm.hpp
│   ├── leg_fsm.cpp
│   ├── servo_controller.hpp
│   ├── servo_controller.cpp
│   ├── input_manager.hpp
│   ├── input_manager.cpp
│   ├── bluepad_manager.hpp
│   ├── bluepad_manager.cpp
│   ├── leg_kinematics.hpp
│   └── leg_kinematics.cpp
│
├── raspberry_pi/
│   ├── main.cpp
│   ├── camera_manager.hpp
│   ├── camera_manager.cpp
│   ├── audio_manager.hpp
│   └── audio_manager.cpp
│
├── common/
│   ├── protocol.hpp
│   └── protocol.cpp
│
├── docs/
│   └── uml/
│       ├── fsm_globale_spiderbot.puml
│       ├── fsm_globale_simplifiee.puml
│       ├── fsm_locomotion_tripod.puml
│       └── fsm_patte.puml
│
├── ARCHITECTURE.md
└── README.md
```

Le point d'entree ESP32 actuellement actif est `esp32/main.cpp`, qui instancie `RobotController`.
La boucle de controle reel se trouve donc dans `esp32/robot_controller.cpp`.
`bluepad_manager.cpp` reste pour l'instant une interface de simulation tant que Bluepad32 n'est pas branche.

---

## Organisation objet actuelle

L'architecture objet actuellement implementee sur ESP32 suit la hierarchie suivante:

```text
RobotController
├── BluePadManager
├── InputManager
│   ├── LocomotionFsm
│   └── LegKinematics
├── RobotFSM
├── TripodWalkFSM
│   └── LegFSM[6]
└── ServoController
```

## Securite implementee (prioritaire)

- Timeout manette centralise dans RobotController (500 ms).
- Perte de signal -> passage en SafeStop + recentrage.
- Defaut servo detecte sur echec d'application des commandes.
- Cible IK impossible detectee et remontee vers la FSM globale.
- Mode Error verrouille par RobotFSM avec arret securise des sorties.

## Rappels de separation des responsabilites

- ESP32: locomotion uniquement (entrees, FSM, IK, servo).
- Raspberry Pi: audio-visuel uniquement (camera, micro, lecture audio).
- La conversion finale vers les servos reste dans ServoController.

## Integration ESP32 (etat actuel)

Les points d'accroche materiels sont maintenant exposes:

- BluePadManager:
	- `publishFrame(const GamepadData&)` pour publier une trame manette depuis un callback SDK.
	- `notifyDisconnected()` pour signaler une perte de manette.
	- `update()` consomme une trame fraiche a la fois et retourne `std::nullopt` sinon.
- ServoController:
	- `setPwmWriteCallback(...)` pour brancher l'ecriture PWM reelle (LEDC ou backend materiel direct).
	- `setPwmDetachCallback(...)` pour le stop/release materiel des sorties.

Un module de wiring runtime est disponible:

- `esp32/esp32_runtime_wiring.hpp`
- `esp32/esp32_runtime_wiring.cpp`

Il est appele automatiquement depuis `esp32/main.cpp` via `configureEsp32RuntimeWiring(controller)`.

Macros optionnelles pour un build ESP32:

- `SPIDERBOT_USE_BLUEPAD32_EXAMPLE` active un exemple de pont Bluepad32 vers `BluePadManager`.
- `SPIDERBOT_USE_LEDC_EXAMPLE` active un exemple de sortie PWM LEDC directe vers `ServoController`.
- Le wiring par defaut utilise un mode hybride de validation sur 6 servos:
	- le tableau de validation par defaut est `0, 1, 2, 9, 10, 11`;
	- patte avant gauche complete (servo 0, 1, 2),
	- patte avant droite complete (servo 9, 10, 11).
	- ordre pratique de test: coxa, femur, tibia pour la patte avant gauche, puis coxa, femur, tibia pour la patte avant droite.

Note importante: l'exemple LEDC natif ESP32 est limite a 16 channels. Sans driver externe,
le mode hybride sert surtout a valider un sous-ensemble de servos avant generalisation.
Pour les 18 servos, il faudra ensuite un backend PWM adapte (multiplexage ou scheduling logiciel).

Tant que ces callbacks ne sont pas relies a du code cible ESP32, le projet reste en mode logique/simulation.
---