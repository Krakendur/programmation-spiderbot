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