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

Source: `esp32/main/main.cpp`

| Patte | Servo IDs | GPIO |
|---|---:|---|
| Avant gauche | 0 / 1 / 2 | 13 / 14 / 15 |
| Milieu gauche | 3 / 4 / 5 | 16 / 17 / 18 |
| Arriere gauche | 6 / 7 / 8 | 19 / 21 / 22 |
| Avant droite | 9 / 10 / 11 | 23 / 25 / 26 |
| Milieu droite | 12 / 13 / 14 | 27 / 32 / 33 |
| Arriere droite | 15 / 16 / 17 | 4 / 5 / 2 |

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

Rappel: le backend hybride utilise `ESP32Servo` via la macro existante `SPIDERBOT_USE_LEDC_EXAMPLE`.
