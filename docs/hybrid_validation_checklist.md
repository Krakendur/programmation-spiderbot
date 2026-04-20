# Checklist terrain - Test hybride (6 servos)

Usage: version courte pour essai sur banc.
Reference detaillee: voir le protocole complet.

Date:
Operateur:
Commit/firmware:
Carte ESP32:

## Stop immediat si

- bruit mecanique continu (butee),
- mouvement non commande (runaway),
- odeur/chauffe anormale,
- reset boucle ou erreur critique repetee.

## Avant allumage

- [ ] Robot sur support (pattes dans le vide)
- [ ] Zone de test degagee
- [ ] Alim servo stable
- [ ] Arret alim accessible en 1 geste
- [ ] Cablage controle sur 6 servos actifs: 0,1,2,9,10,11

## Gate 0 - Sanity desktop

- [ ] Build desktop OK
- [ ] Run simulation OK
- [ ] Logs vus: REST, FORWARD, TURN, CENTER, WALK_LIGHT
- [ ] Logs FSM vus: mode TELEOP, ik=no

Resultat Gate 0: [ ] PASS [ ] FAIL

## Gate 1 - Boot hybride ESP32

- [ ] Flash firmware hybride
- [ ] Boot stable (pas de passage immediat en ERROR)
- [ ] Log backend hybride visible
- [ ] IDs servos de validation affiches (0,1,2,9,10,11)

Resultat Gate 1: [ ] PASS [ ] FAIL

## Gate 2 - Mouvement des 6 servos

Ordre de test pratique:

- [ ] 0 coxa avant gauche
- [ ] 1 femur avant gauche
- [ ] 2 tibia avant gauche
- [ ] 9 coxa avant droite
- [ ] 10 femur avant droite
- [ ] 11 tibia avant droite
- [ ] Avance legere
- [ ] Rotation legere
- [ ] Recentrage

Controles:

- [ ] Mouvement fluide
- [ ] Pas d'oscillation anormale
- [ ] Pas de blocage mecanique
- [ ] FSM reste en TELEOP avec signal present

Resultat Gate 2: [ ] PASS [ ] FAIL

## Gate 3A - Perte signal manette

- [ ] Couper manette / flux
- [ ] Verification timeout ~500 ms
- [ ] Log safe stop visible
- [ ] Transition SAFE_STOP visible
- [ ] Recentrage effectif

Resultat Gate 3A: [ ] PASS [ ] FAIL

## Gate 3B - Defaut servo injecte

- [ ] Build avec macro SPIDERBOT_TEST_SERVO_FAULT
- [ ] Defaut servo detecte
- [ ] Transition ERROR visible
- [ ] Arret securise des sorties effectif

Resultat Gate 3B: [ ] PASS [ ] FAIL

## Gate 3C - IK impossible (si prepare)

- [ ] Injection cible IK impossible activee
- [ ] Transition ERROR visible
- [ ] Arret securise effectif

Resultat Gate 3C: [ ] PASS [ ] FAIL [ ] N/A

## Bilan final

- [ ] GO essai suivant
- [ ] NO-GO correction necessaire

Notes:
