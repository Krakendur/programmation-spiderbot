# Architecture Spider-Bot - ESP32 + Raspberry Pi Zero 2 W

## Vue d'ensemble

Le projet est compose de deux sous-systemes independants:

- Sous-systeme mouvement (ESP32): gere uniquement la manette et les 18 servomoteurs.
- Sous-systeme audio-visuel (Raspberry Pi Zero 2 W): gere uniquement camera, micro et haut-parleur.

Il n'y a pas de communication fonctionnelle ESP32 <-> Raspberry Pi dans cette version.

```
┌──────────────────────────────────────────────────────────────┐
│             Spider-Bot - Architecture Separee               │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌────────────────────┐                                      │
│  │  Manette Wireless  │                                      │
│  │  Bluepad32 2.4GHz  │                                      │
│  └────────┬───────────┘                                      │
│           │ Bluetooth 2.4GHz                                 │
│           ▼                                                  │
│  ┌────────────────────────────────────────────┐              │
│  │            ESP32 (Mouvement)              │              │
│  │  ┌──────────────────────────────────────┐  │              │
│  │  │ • Bluepad Manager                    │  │              │
│  │  │ • Input Manager                      │  │              │
│  │  │ • Leg Kinematics (IK + tripod)       │  │              │
│  │  │ • Servo Controller (PWM 18x)         │  │              │
│  │  └──────────────────────────────────────┘  │              │
│  └────────────┬───────────────────────────────┘              │
│               │ GPIO PWM (18 pins)                            │
│               ▼                                               │
│  ┌──────────────────────────────────────────┐                  │
│  │ 18 Servomoteurs MG996R                   │                  │
│  └──────────────────────────────────────────┘                  │
│                                                              │
│  ┌──────────────────────────────────────────┐                  │
│  │ Raspberry Pi Zero 2 W (Audio-Visuel)    │                  │
│  │ • Camera                                 │                  │
│  │ • Microphone                             │                  │
│  │ • Haut-parleur                           │                  │
│  └──────────────────────────────────────────┘                  │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

## Composants

### Cartes de Developpement
- ESP32 - Controle mouvement (servos + manette)
- Raspberry Pi Zero 2 W - Systeme audio-visuel

### Controle
- Manette Bluepad32 - Sans fil 2.4GHz (Bluetooth)
- Aucun lien de commande ESP32 <-> Raspberry Pi dans cette version

### Actionneurs
- 18x Servomoteurs MG996R - Controle mouvement

### Electronique de Puissance
- 3x Convertisseur XL4016 - Alimentation servos
- 1x Convertisseur K240505 - Alimentation Raspberry Pi
- 1x Batterie LiPo 3S - 11.1V, 2200mAh, 50C

### Audio-Visuel
- Camera Module 3 Wide - CSI
- Microphone INMP441 - I2S
- Amplificateur MAX98357A - I2S
- Haut-parleur AIYIMA - 4 ohms 10W

---

## Architecture Logicielle ESP32

### Hierarchie FSM et orchestration

```text
RobotController
├── BluePadManager
├── InputManager
├── RobotFSM (globale)
│   ├── TripodWalkFSM (locomotion)
│   │   ├── LegFSM x6 (une par patte)
├── LegKinematics (IK)
└── ServoController (sortie servo finale)
```

- La FSM globale supervise le mode: Idle, Teleop, SafeStop, Error.
- TripodWalkFSM pilote l'activation de la marche et les phases des 6 pattes.
- LegKinematics reste le moteur mathematique central de l'IK.
- ServoController conserve la conversion finale vers les 18 sorties servo.

### Modules

| Module | Role |
|--------|------|
| robot_controller.cpp/.hpp | Orchestration globale de la boucle 50Hz |
| robot_fsm.cpp/.hpp | Supervision des modes et transitions de securite |
| tripod_walk_fsm.cpp/.hpp | Supervision locomotion tripod |
| leg_fsm.cpp/.hpp | FSM legere Support/Transfer par patte |
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

### Garde-fous de securite

- Timeout manette: 500 ms, centralise dans RobotController.
- Signal perdu: passage en SafeStop et recentrage.
- Defaut servo: detection sur echec d'ecriture des positions.
- Cible IK impossible: remontee d'erreur vers RobotFSM.
- Erreur critique: recentrage, stop PWM et sortie securisee.

---

## Architecture Logicielle Raspberry Pi Zero 2 W

### Modules

| Module | Role |
|--------|------|
| camera_manager.cpp/.hpp | Gestion capture camera |
| audio_manager.cpp/.hpp | Gestion microphone et lecture audio |
| main.cpp | Boucle principale audio-visuelle |

### Flux de Donnees

```
CAMERA / MICRO
    ↓
Raspberry Pi Zero 2 W
    ↓
Traitement / lecture locale audio-visuelle
```

---

## Specifications

### Boucle Principale ESP32
- Frequence: 50Hz (20ms)
- Protocole servos: PWM 50Hz
- Timeout Bluepad: 500ms -> auto-center

### Entrees Bluepad32
- Sticks: [-1.0, 1.0]
- Triggers: [0.0, 1.0]
- Boutons: Digital
- Frequence: 50Hz

### Alimentation
- Servos: ~50W max (mouvement)
- ESP32: ~0.5W
- Pi Zero 2 W: ~1-2W
- Total: ~55W max, ~5W idle
- Autonomie: 30-40 min LiPo 3S

---

## Remarque de Separation des Systemes

- ESP32 et Raspberry Pi Zero 2 W sont documentes comme deux blocs independants.
- L'audio-visuel est exclusivement gere par la Raspberry Pi Zero 2 W.
- Le mouvement des pattes est exclusivement gere par l'ESP32.
