# Spider-Bot - README développeur orienté GitHub Copilot

## Vue d’ensemble

Le projet **Spider-Bot** est un robot hexapode téléopéré en **C++**, organisé autour de deux sous-systèmes indépendants :

- **ESP32** : gestion de la manette, logique de locomotion, FSM, cinématique inverse et pilotage des **18 servomoteurs** ;
- **Raspberry Pi Zero 2 W** : gestion de la partie **audio-visuelle** (caméra, microphones, amplificateur et haut-parleurs).

Dans la version actuelle, ces deux sous-systèmes sont **séparés** :
- l’ESP32 s’occupe uniquement du **mouvement** ;
- la Raspberry Pi s’occupe uniquement de l’**audio/vidéo** ;
- il n’y a pas encore de **communication fonctionnelle active** entre eux.

Ce README a pour but de :
- documenter l’architecture du projet ;
- expliquer la logique des FSM ;
- fournir à **GitHub Copilot** un contexte clair pour générer du code cohérent avec le Spider-Bot.

---

## Objectif du projet

Le Spider-Bot est un robot d’infiltration inspiré de l’univers Spider-Man.  
Il doit pouvoir :
- se déplacer sur différents terrains ;
- rester compact ;
- être téléopéré à distance ;
- fournir un retour visuel en temps réel ;
- intégrer une architecture de locomotion stable et réaliste.

Dans la version hexapode étudiée :
- le robot possède **6 pattes** ;
- chaque patte possède **3 degrés de liberté** ;
- le système totalise **18 servomoteurs**.

---

## Structure logique du projet

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

---

## Organisation objet actuelle

L'architecture objet actuellement implementee sur ESP32 suit la hierarchie suivante:

```text
RobotController
├── BluePadManager
├── InputManager
├── RobotFSM
│   ├── TripodWalkFSM
│   │   ├── LegFSM[6]
├── LegKinematics
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
---