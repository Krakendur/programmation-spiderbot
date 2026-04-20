# Protocole de test hybride materiel (6 servos)

Date de reference: 2026-04-20

Checklist courte pour usage banc: `docs/hybrid_validation_checklist.md`

## Objectif

Valider sur banc la chaine complete ESP32 pour un sous-ensemble de 6 servos:

- entree manette,
- FSM globale + locomotion,
- IK,
- sortie servo via backend PWM reel (mode hybride).

Le test ne couvre pas la partie Raspberry Pi (audio/video).

## Perimetre de validation

Le mode hybride par defaut active uniquement les servos:

- 0: front-left coxa
- 1: front-left femur
- 2: front-left tibia
- 9: front-right coxa
- 10: front-right femur
- 11: front-right tibia

Reference code:

- `esp32/esp32_runtime_wiring.hpp`: `kDefaultHybridValidationServoIds`
- `esp32/esp32_runtime_wiring.cpp`: mode `Esp32WiringMode::HybridValidation`

## Prerequis

## Prerequis logiciels

- Build desktop passe (sanity check logique).
- Build ESP32 avec backend LEDC d'exemple active.
- Optionnel: build ESP32 avec backend Bluepad32 d'exemple active.

## Prerequis banc materiel

- Robot sur support (pattes dans le vide, aucune charge au sol).
- Alimentation servo stable et protegee.
- Bouton/commande d'arret alimentation accessible immediatement.
- Cablage controle pour les 6 servos actifs uniquement pendant ce test.

## Gate 0 - Sanity check desktop

Depuis la racine du repo:

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic .\esp32\*.cpp .\common\*.cpp -I.\esp32 -I.\common -o .\spiderbot_esp32_sim_check.exe
.\spiderbot_esp32_sim_check.exe
```

Attendus minimum dans les logs:

- `[SIM] phase=REST`, puis `FORWARD`, `TURN`, `CENTER`, `WALK_LIGHT`
- `[FSM] mode=TELEOP ... ik=no`
- `[STATUS] loop=... bluepad=CONNECTED mode=TELEOP`

## Gate 1 - Demarrage hybride sur ESP32

1. Flasher le firmware ESP32 configure pour le mode hybride LEDC.
2. Demarrer sans poser le robot au sol.
3. Verifier dans les logs:
   - activation backend LEDC hybride,
   - affichage des 6 IDs servos de validation,
   - absence de passage immediat en `ERROR`.

Critere GO Gate 1:

- Le systeme reste stable en boucle et publie des logs FSM/STATUS.

## Gate 2 - Sequence fonctionnelle des 6 servos

Executer les commandes manette dans cet ordre pratique:

1. Patte avant gauche: coxa (0), femur (1), tibia (2).
2. Patte avant droite: coxa (9), femur (10), tibia (11).
3. Commande avance legere.
4. Commande rotation legere.
5. Commande recentrage.

Points de controle:

- Mouvement fluide, sans oscillation anormale.
- Pas de blocage mecanique ni bruit de butee permanent.
- FSM reste en `TELEOP` tant que le signal manette est present.

Critere GO Gate 2:

- 6/6 servos de validation repondent correctement aux commandes.

## Gate 3 - Tests securite obligatoires

## 3A - Perte de signal manette

1. Couper la manette ou interrompre volontairement le flux.
2. Attendre le timeout (500 ms, `kBluepadTimeoutMs`).

Attendus:

- log `Signal lost, applying safe stop`
- transition FSM vers `SAFE_STOP`
- recentrage via `centerAll()`

## 3B - Defaut servo injecte

1. Compiler avec macro `SPIDERBOT_TEST_SERVO_FAULT`.
2. Redemarrer et observer la reaction.

Attendus:

- detection defaut servo
- passage FSM en `ERROR`
- arret securise des sorties (`stopAll()`)

## 3C - Cible IK inatteignable

Etat actuel:

- la remontee d'erreur IK vers la FSM globale est implementee,
- aucun point d'injection direct n'est expose dans le wiring runtime.

Recommandation execution:

- preparer une branche de test qui force une cible impossible dans la chaine IK,
- verifier ensuite la transition en `ERROR` et l'arret securise.

## Fiche de releve (a remplir pendant test)

```text
Date/heure:
Firmware/commit:
Carte ESP32:
Alim servos:

Gate 0 (desktop): PASS/FAIL
Gate 1 (demarrage hybride): PASS/FAIL
Gate 2 (6 servos): PASS/FAIL
Gate 3A (signal loss): PASS/FAIL
Gate 3B (servo fault): PASS/FAIL
Gate 3C (IK impossible): PASS/FAIL / N/A

Observations:
Actions correctives:
```

## Criteres de sortie

- Tous les tests obligatoires Gate 0, Gate 1, Gate 2, Gate 3A, Gate 3B sont PASS.
- Aucun comportement dangereux observe (runaway, butee continue, reset non controle).
- Si Gate 3C non execute, garder le statut "pret partiel" et planifier l'injection IK.