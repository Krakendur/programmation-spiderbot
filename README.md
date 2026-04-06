# Spider-Bot - Programmation C++

## Vue d'ensemble

Cette version du projet est migree officiellement en C++.

Le dossier contient:
- code ESP32 en C++ pour la commande des 18 servos
- cinematique inverse des pattes avec demarche tripod
- code Raspberry Pi en C++ pour la couche audio/video
- protocole de communication commun en C++

## Structure du projet

```text
programmation spiderbot/
├── esp32/
│   ├── main.cpp
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
├── ARCHITECTURE.md
└── README.md
```

## Notes importantes

- Les anciens fichiers Python ont ete retires pour acter la migration.
- Certains modules materiels sont fournis en base C++ (stubs) et doivent etre relies
  a vos bibliotheques cibles:
  - ESP32: PWM, UART, Bluepad32/SDK
  - Raspberry Pi: camera et audio (libcamera/ALSA ou equivalent)

## Cinetique des pattes

Le module de cinetique est dans:
- esp32/leg_kinematics.hpp
- esp32/leg_kinematics.cpp

Parametres mecaniques utilises:
- coxa: 46 mm
- femur: 87 mm
- tibia: 126 mm

La demarche tripod est incluse avec deux groupes en opposition de phase.

## Prochaine etape recommandee

Ajouter une chaine de build CMake (ou ESP-IDF + CMake) pour compiler:
- la cible ESP32
- la cible Raspberry Pi
