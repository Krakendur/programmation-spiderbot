# Guide pratique - Manette PS5 et branchement servos ESP32

Date de reference: 2026-04-20

## 1) Connecter une manette PS5 (DualSense) au Spider-Bot

But: utiliser la manette reellement, au lieu de la simulation de trames.

### Etape A - Activer le backend manette cote firmware

- Compiler/flasher la cible ESP32 avec la macro SPIDERBOT_USE_BLUEPAD32_EXAMPLE activee.
- Au boot, verifier le log attendu:
  - [ESP32] Bluepad32 example backend active

Si le log affiche Bluepad32 backend disabled, le robot est encore en mode simulation d'entree.

### Etape B - Mettre la DualSense en mode appairage

- Manette eteinte.
- Maintenir PS + Create jusqu'a clignotement rapide.
- Attendre la connexion cote ESP32.

### Etape C - Verifier les logs de fonctionnement

- [FSM] mode=TELEOP ... frame=yes ...
- [STATUS] ... bluepad=CONNECTED ...

Si frame=no en continu, la manette n'envoie pas de trame exploitee.

## 2) Ou brancher les servos sur l'ESP32

Le mapping actuel du code est defini par la liste servo_id -> GPIO.

Source: esp32/main.cpp

- servo 0  -> GPIO 2
- servo 1  -> GPIO 4
- servo 2  -> GPIO 5
- servo 3  -> GPIO 12
- servo 4  -> GPIO 13
- servo 5  -> GPIO 14
- servo 6  -> GPIO 15
- servo 7  -> GPIO 16
- servo 8  -> GPIO 17
- servo 9  -> GPIO 18
- servo 10 -> GPIO 19
- servo 11 -> GPIO 21
- servo 12 -> GPIO 22
- servo 13 -> GPIO 23
- servo 14 -> GPIO 25
- servo 15 -> GPIO 26
- servo 16 -> GPIO 27
- servo 17 -> GPIO 32

## 3) Branchement electrique obligatoire (important)

- Ne pas alimenter les servos depuis le 5V de l'ESP32.
- Utiliser une alimentation servo externe adaptee (courant suffisant).
- Relier la masse alim servo et la masse ESP32 ensemble (GND commun).
- Le fil signal de chaque servo va sur le GPIO du tableau ci-dessus.

Sans GND commun, le signal PWM peut devenir instable.

## 4) Demarrage recommande (safe)

1. Commencer en mode hybride 6 servos (0,1,2,9,10,11).
2. Robot sur support, pattes dans le vide.
3. Verifier recentrage puis commandes legeres.
4. Etendre ensuite progressivement.

Rappel: l'exemple LEDC natif couvre 16 channels. Pour 18 servos en reel, prevoir un backend PWM adapte.