# Checklist terrain - Test DualSense + 6 servos

Profil a flasher pour le banc actuel: `esp32dev-bt-test`.

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

## Gate 0 - Build

- [ ] `pio run -e esp32dev-bt-test` OK
- [ ] Pas d'erreur LEDC
- [ ] Pas d'erreur Bluepad32

Resultat Gate 0: [ ] PASS [ ] FAIL

## Gate 1 - Boot ESP32

- [ ] Flash firmware `esp32dev-bt-test`
- [ ] Boot stable
- [ ] Log `[BT] Spider-Bot Bluepad32 + 6 servos test demarre`
- [ ] IDs servos de validation affiches (0,1,2,9,10,11)
- [ ] Log `[SERVO] 6 servos centres a 90 degres`

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
- [ ] Logs `[PAD]` coherents avec les mouvements de la manette

Resultat Gate 2: [ ] PASS [ ] FAIL

## Gate 3 - Deconnexion manette

- [ ] Couper manette / flux
- [ ] Log `[BT] Manette deconnectee`
- [ ] Recentrage effectif

Resultat Gate 3: [ ] PASS [ ] FAIL

## Bilan final

- [ ] GO essai suivant
- [ ] NO-GO correction necessaire

Notes:
