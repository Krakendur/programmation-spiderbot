# Guide pratique - Manette PS5 et branchement servos ESP32

Chemin de test actuel recommande:

```powershell
cd esp32
pio run -e esp32dev-bt-test -t upload
pio device monitor -b 115200
```

Ce profil valide la DualSense et les 6 servos de banc dans `esp32/main/bt_test_main.c`.

Date de reference: 2026-04-20

## 1) Connecter une manette PS5 (DualSense) au Spider-Bot

But: utiliser la manette reellement, au lieu de la simulation de trames.

### Etape A - Activer le backend manette cote firmware

- Compiler/flasher la cible ESP32 `esp32dev-bt-test`.
- Au boot, verifier le log attendu:
  - [BT] Spider-Bot Bluepad32 + 6 servos test demarre
  - [BT] Bluetooth pret
  - [BT] DualSense prete!

Si le log affiche `Bluepad32 backend disabled`, tu n'es pas sur le profil de test ESP-IDF pur. Revenir au profil `esp32dev-bt-test` pour le banc manette + 6 servos.

### Etape B - Mettre la DualSense en mode appairage

- Manette eteinte.
- Maintenir PS + Create jusqu'a clignotement rapide.
- Attendre la connexion cote ESP32.

### Etape C - Verifier les logs de fonctionnement

- [BT] Origine sticks capturee: ...
- [PAD] lx= ... ly= ... rx= ... ry= ...
- [SERVO] 6 servos centres a 90 degres

Si les logs `[PAD]` n'apparaissent pas apres `DualSense prete`, la manette est connectee mais les trames ne remontent pas correctement.

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

1. Commencer avec `esp32dev-bt-test` sur 6 servos (0,1,2,9,10,11).
2. Robot sur support, pattes dans le vide.
3. Verifier recentrage puis commandes legeres.
4. Etendre ensuite progressivement.

Rappel: le test actuel utilise directement LEDC dans `bt_test_main.c` pour rester sur le chemin Bluepad32 deja valide. Le backend robot complet via `ESP32Servo` reste pour l'etape suivante.
