# Architecture Spider-Bot - Version actuelle

## Vue d'ensemble

Le projet Spider-Bot est un robot hexapode en C++ organise autour de deux sous-systemes independants:

- ESP32: manette, calcul de mouvement, cinematique inverse, demarche et commande des 18 servomoteurs
- Raspberry Pi Zero 2 W: audio-visuel (camera, microphone, haut-parleur)

Dans la version actuelle, les deux blocs sont separes:

- ESP32 = mouvement uniquement
- Raspberry Pi = audio/video uniquement
- pas de communication fonctionnelle active ESP32 <-> Raspberry Pi

---

## Architecture globale

```text
                                                 MANETTE (Bluetooth)
                                                                |
                                                                v
                                     +---------------------------+
                                     |   ESP32 - Locomotion      |
                                     |---------------------------|
                                     | bluepad_manager           |
                                     | input_manager             |
                                     | leg_kinematics            |
                                     | servo_controller          |
                                     +-------------+-------------+
                                                                 |
                                                                 v
                                                18 Servomoteurs


                                     +---------------------------+
                                     | Raspberry Pi Zero 2 W     |
                                     |---------------------------|
                                     | camera_manager            |
                                     | audio_manager             |
                                     +-------------+-------------+
                                                                 |
                                                                 v
                                                Camera / Micro / HP

                     (Aucun canal de communication actif entre les 2 blocs)
```

---

## Locomotion - modele patte

Le robot a 6 pattes avec 3 articulations par patte:

- coxa: rotation horizontale
- femur: levage/deploiement
- tibia: hauteur et extension terminale

Total actionneurs: 6 x 3 = 18 servomoteurs.

Pipeline de locomotion:

1. Reception de la consigne utilisateur (manette)
2. Conversion en cibles de pieds (x, y, z)
3. Calcul des angles articulaires (cinematique inverse)
4. Emission des commandes PWM vers les 18 servos

---

## Architecture Logicielle ESP32

### Modules

| Module | Role |
|--------|------|
| servo_controller.cpp/.hpp | Gestion PWM 18 servos (50Hz) |
| input_manager.cpp/.hpp | Traitement entrees manette + mapping cinematique |
| bluepad_manager.cpp/.hpp | Interface manette Bluepad |
| leg_kinematics.cpp/.hpp | Cinematique inverse + demarche tripod |
| main.cpp | Boucle principale ESP32 |

### Flux de Donnees

```
MANETTE BLUEPAD32
    ↓ Bluetooth 2.4GHz
    └─→ ESP32
        ↓
        Input Manager
        ↓
        Leg Kinematics
        ↓
        Servo Controller
        ↓
        PWM GPIO (18x)
        ↓
    SERVOMOTEURS MG996R
```

### Priorites d'Entree

1. Manette Bluepad32 (principale si connectee)
2. Position neutre (timeout auto 500ms)

---

## Architecture logicielle Raspberry Pi

| Module | Role |
|--------|------|
| main.cpp | Boucle principale audio-visuelle |
| camera_manager.cpp/.hpp | Capture et gestion camera |
| audio_manager.cpp/.hpp | Entree micro et sortie audio |

Flux logique:

```text
Camera/Micro
    -> camera_manager / audio_manager
    -> traitement local Raspberry Pi
    -> sortie audio/video locale
```

---

## Espace commun

Le dossier common contient une base de protocole partagee:

- protocol.hpp
- protocol.cpp

Ce module prepare l'evolution vers une communication inter-cartes, mais cette communication n'est pas encore active dans l'etat actuel.

---

## Lien avec la future FSM

Le code actuel reste modularise par responsabilite (input, kinematics, servo, audio, camera). Cette decomposition est compatible avec une evolution vers une architecture FSM vue en cours.

Projection FSM (non implementee integralement a ce stade):

- Etats locomotion potentiels: Idle, Stand, Walk, Turn, EmergencyStop
- Etats audio-visuels potentiels: IdleAV, Capture, Record, Playback
- Evenements futurs: ordre utilisateur, timeout, perte manette, defaut composant

Objectif: formaliser les transitions d'etat sans casser les modules deja en place.
