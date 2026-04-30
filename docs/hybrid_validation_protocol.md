# Protocole de test DualSense + 6 servos

Checklist courte pour usage banc: `docs/hybrid_validation_checklist.md`

## Objectif

Valider rapidement la chaine materielle minimale avant de revenir au robot complet:

- appairage DualSense via Bluepad32,
- capture de l'origine des joysticks a la connexion,
- sortie PWM LEDC directe sur 6 servos,
- recentrage au demarrage, au bouton `X` et a la deconnexion.

Ce test ne couvre pas encore la FSM globale, la locomotion tripode, ni l'IK. Ces couches restent dans le robot C++ autour de `main.cpp`.

## Profil a utiliser

Depuis `esp32/`:

```powershell
pio run -e esp32dev-bt-test
pio run -e esp32dev-bt-test -t upload
pio device monitor -b 115200
```

Code execute: `esp32/main/bt_test_main.c`.

## Servos actifs

Le banc active uniquement:

- 0: front-left coxa, GPIO 13
- 1: front-left femur, GPIO 14
- 2: front-left tibia, GPIO 15
- 9: front-right coxa, GPIO 23
- 10: front-right femur, GPIO 25
- 11: front-right tibia, GPIO 26

Les angles sont limites a `60..120 deg` autour du centre `90 deg`.

## Logs attendus

```text
[BT] Spider-Bot Bluepad32 + 6 servos test demarre
[SERVO] servo 0 GPIO 13 pret (...)
[SERVO] 6 servos centres a 90 degres
[BT] Bluetooth pret
[BT] DualSense prete!
[BT] Origine sticks capturee: ...
[PAD] lx= ...
```

## Commandes de test

- Stick gauche X: coxa gauche/droite.
- Stick gauche Y: femur/tibia.
- Stick droit X: differentiel de yaw entre les deux pattes avant.
- Stick droit Y: hauteur legere.
- R2/L2: offset de levee.
- Bouton `X`: recentrage des 6 servos.

## Criteres GO

- Les 6 servos se centrent au boot.
- La DualSense se connecte sans crash.
- Les logs `[PAD]` suivent les joysticks.
- Les mouvements restent fluides, limites et sans butee.
- La deconnexion recentre les servos.

## Etape suivante

Une fois ce banc valide, reintegrer la manette reelle dans le chemin robot complet:

```text
DualSense -> BluePadManager -> InputManager -> FSM/IK -> ServoController
```

Le profil vise pour cette etape est `esp32dev-ps5-hybrid`, mais il faut d'abord resoudre proprement la disponibilite du backend Arduino `Bluepad32.h` ou porter le pont Bluepad32 ESP-IDF vers le runtime C++.
