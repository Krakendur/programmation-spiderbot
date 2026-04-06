#include "leg_kinematics.hpp"

#include <cmath>

namespace spiderbot {

// =============================================================================
// Glossaire des acronymes utilises dans ce fichier
// =============================================================================
// IK      : Inverse Kinematics (cinematique inverse)
// DDL     : Degre de Liberte (nombre d'articulations independantes)
// PWM     : Pulse Width Modulation (signal de commande des servos)
// FL/ML/RL: Front/Middle/Rear Left  (avant/milieu/arriere gauche)
// FR/MR/RR: Front/Middle/Rear Right (avant/milieu/arriere droite)
//
// Fonctionnement global:
// 1) On recoit une commande de deplacement normalisee (forward/lateral/yaw/height/lift).
// 2) On calcule une position cible du pied pour chaque patte (repere local).
// 3) IK: on convertit cette cible (x,y,z) en angles articulaires (coxa/femur/tibia).
// 4) Calibration servo: offsets + sens + limites -> angles servos [0..180].
// 5) Le tableau final de 18 angles est envoye a ServoController (qui pilotera la PWM).
// =============================================================================

namespace {

// Constante PI locale pour toutes les conversions rad <-> deg.
constexpr double kPi = 3.14159265358979323846;

double safeAcos(double x) {
    // Protection numerique: evite NaN quand les arrondis depassent [-1, 1].
    if (x < -1.0) {
        x = -1.0;
    }
    if (x > 1.0) {
        x = 1.0;
    }
    return std::acos(x);
}

}  // namespace

// -----------------------------------------------------------------------------
// Constructeur / parametrage robot
// -----------------------------------------------------------------------------
// On initialise ici:
// - les dimensions mecaniques (coxa, femur, tibia)
// - la geometrie des pattes sur le chassis
// - la calibration servo (centres, sens, limites)
//
// Convention d'ordre des pattes:
// 0: FL (front-left), 1: ML, 2: RL, 3: FR, 4: MR, 5: RR
//
// Detail calibration:
// - servoCenters_: angle "zero mecanique" de chaque articulation
// - servoSigns_  : +1 ou -1 selon le sens de montage reel du servo
// - servoLimits_ : bornes de securite pour proteger la mecanique
// -----------------------------------------------------------------------------
LegKinematics::LegKinematics()
    : coxaLen_(46.0),
      femurLen_(87.0),
      tibiaLen_(126.0),
      legAnchors_({{{70.0, 80.0}, {0.0, 95.0}, {-70.0, 80.0}, {70.0, -80.0}, {0.0, -95.0}, {-70.0, -80.0}}}),
      neutralFootLocal_({{{120.0, 0.0, -95.0},
                          {125.0, 0.0, -95.0},
                          {120.0, 0.0, -95.0},
                          {120.0, 0.0, -95.0},
                          {125.0, 0.0, -95.0},
                          {120.0, 0.0, -95.0}}}),
      servoCenters_({{{90, 90, 90}, {90, 90, 90}, {90, 90, 90}, {90, 90, 90}, {90, 90, 90}, {90, 90, 90}}}),
      servoSigns_({{{+1, +1, +1}, {+1, +1, +1}, {+1, +1, +1}, {-1, -1, -1}, {-1, -1, -1}, {-1, -1, -1}}}),
      servoLimits_({{{{{0, 180}, {0, 180}, {0, 180}}},
                     {{{0, 180}, {0, 180}, {0, 180}}},
                     {{{0, 180}, {0, 180}, {0, 180}}},
                     {{{0, 180}, {0, 180}, {0, 180}}},
                     {{{0, 180}, {0, 180}, {0, 180}}},
                     {{{0, 180}, {0, 180}, {0, 180}}}}}) {}

// Borne utilitaire reutilisee partout (entrees joystick, angles, distances).
double LegKinematics::clamp(double value, double low, double high) {
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

// Convertit le temps absolu en phase normalisee [0,1) du cycle de marche.
double LegKinematics::phase(long long timeMs, int cycleMs) {
    if (cycleMs <= 0) {
        return 0.0;
    }
    const long long modulo = timeMs % cycleMs;
    return static_cast<double>(modulo) / static_cast<double>(cycleMs);
}

// Courbe lineaire de phase support: 1 -> -1.
// Interprete comme "avance relative du pied" pendant la poussee.
double LegKinematics::supportCurve(double u) {
    return 1.0 - 2.0 * u;
}

// Courbe lineaire de phase transfert: -1 -> 1.
// Interprete comme "retour du pied" pendant la phase aerienne.
double LegKinematics::transferCurve(double u) {
    return -1.0 + 2.0 * u;
}

// -----------------------------------------------------------------------------
// IK locale d'une patte (3 DDL)
// -----------------------------------------------------------------------------
// Entrees:
// - x, y, z dans le repere local de la patte (en mm)
// Sortie:
// - angles articulaires geometriques (coxa, femur, tibia) en degres
//
// Idee generale:
// 1) orienter la coxa dans le plan horizontal
// 2) reduire le probleme a un triangle 2D (femur/tibia)
// 3) appliquer la loi des cosinus
//
// Convention de repere (patte locale):
// - x: vers l'avant de la patte
// - y: vers l'exterieur du robot
// - z: vers le haut (donc pied au sol => z negatif)
// -----------------------------------------------------------------------------
std::array<double, 3> LegKinematics::solveLegIk(double x, double y, double z) const {
    // Etape 1: orientation horizontale (coxa).
    const double coxaDeg = std::atan2(y, x) * 180.0 / kPi;

    // Etape 2: reduction au plan 2D femur/tibia.
    // On retire la longueur de coxa pour isoler la partie "plan vertical".
    double horizontal = std::sqrt(x * x + y * y) - coxaLen_;
    if (horizontal < 1e-6) {
        horizontal = 1e-6;
    }

    // Etape 3: clamp de la distance cible a la zone atteignable de la patte.
    // Sans ce clamp, une cible hors zone pourrait produire des calculs invalides.
    double d = std::sqrt(horizontal * horizontal + z * z);
    const double maxReach = femurLen_ + tibiaLen_ - 1e-6;
    const double minReach = std::abs(femurLen_ - tibiaLen_) + 1e-6;
    d = clamp(d, minReach, maxReach);

    // alpha: inclinaison de la cible dans le plan femur/tibia.
    // beta : correction geometrique pour rejoindre exactement la distance d.
    const double alpha = std::atan2(z, horizontal);

    const double beta = safeAcos((femurLen_ * femurLen_ + d * d - tibiaLen_ * tibiaLen_) /
                                 (2.0 * femurLen_ * d));
    const double femurDeg = (alpha + beta) * 180.0 / kPi;

    // kneeInternal: angle interne du triangle au niveau "genou".
    // On convertit ensuite vers un angle tibia exploitable pour le controle.
    const double kneeInternal =
        safeAcos((femurLen_ * femurLen_ + tibiaLen_ * tibiaLen_ - d * d) /
                 (2.0 * femurLen_ * tibiaLen_));
    const double tibiaDeg = (kPi - kneeInternal) * 180.0 / kPi;

    return {coxaDeg, femurDeg, tibiaDeg};
}

// -----------------------------------------------------------------------------
// Conversion angles articulaires -> angles servos
// -----------------------------------------------------------------------------
// On applique, par patte et par articulation:
// - un offset (centre servo)
// - un signe (sens de montage)
// - une saturation (limites mecaniques)
//
// Pourquoi cette etape est indispensable:
// - l'IK donne des angles geometriques "ideaux"
// - le robot reel a des montages asymetriques et des butees mecaniques
// -----------------------------------------------------------------------------
std::array<int, 3> LegKinematics::jointToServo(int legId,
                                                double coxaDeg,
                                                double femurDeg,
                                                double tibiaDeg) const {
    const auto& centers = servoCenters_[legId];
    const auto& signs = servoSigns_[legId];
    const auto& limits = servoLimits_[legId];

    const std::array<double, 3> raw = {
        static_cast<double>(centers[0]) + signs[0] * coxaDeg,
        static_cast<double>(centers[1]) + signs[1] * femurDeg,
        static_cast<double>(centers[2]) + signs[2] * tibiaDeg,
    };

    std::array<int, 3> result{};
    for (int i = 0; i < 3; ++i) {
        // Clamp final pour ne jamais envoyer de consigne hors plage servo.
        result[i] = static_cast<int>(
            clamp(raw[i], static_cast<double>(limits[i][0]), static_cast<double>(limits[i][1])));
    }

    return result;
}

std::array<int, LegKinematics::NUM_SERVOS> LegKinematics::computeServoAngles(double forward,
                                                                              double lateral,
                                                                              double yaw,
                                                                              double height,
                                                                              double lift) const {
    // Mode statique: applique une pose corps sans cycle de marche.
    // Ces gains convertissent des commandes normalisees en deplacements mm/rad.
    const double maxXYShift = 30.0;
    const double maxYawRad = 18.0 * kPi / 180.0;
    const double maxHeightShift = 28.0;
    const double maxLift = 25.0;

    const double dx = clamp(forward, -1.0, 1.0) * maxXYShift;
    const double dy = clamp(lateral, -1.0, 1.0) * maxXYShift;
    const double yawRad = clamp(yaw, -1.0, 1.0) * maxYawRad;
    const double dz = clamp(height, -1.0, 1.0) * maxHeightShift;
    const double dzLift = clamp(lift, 0.0, 1.0) * maxLift;

    std::array<int, NUM_SERVOS> out{};
    out.fill(90);

    // Chaque patte est calculee independamment, puis mappee vers 3 servos.
    for (int legId = 0; legId < NUM_LEGS; ++legId) {
        const double baseX = legAnchors_[legId][0];
        const double baseY = legAnchors_[legId][1];
        const double nfx = neutralFootLocal_[legId][0];
        const double nfy = neutralFootLocal_[legId][1];
        const double nfz = neutralFootLocal_[legId][2];

        // Approximation petits angles de rotation corps autour de Z (yaw):
        // dX = -yaw * y, dY = yaw * x
        const double yawDx = -yawRad * baseY;
        const double yawDy = yawRad * baseX;

        const double tx = nfx - dx - yawDx;
        const double ty = nfy - dy - yawDy;
        const double tz = nfz - dz + dzLift;

        // IK locale puis conversion vers angles servo physiques.
        const auto joint = solveLegIk(tx, ty, tz);
        const auto servo = jointToServo(legId, joint[0], joint[1], joint[2]);

        const int i = legId * SERVOS_PER_LEG;
        out[i] = servo[0];
        out[i + 1] = servo[1];
        out[i + 2] = servo[2];
    }

    return out;
}

std::array<int, LegKinematics::NUM_SERVOS> LegKinematics::computeTripodServoAngles(double forward,
                                                                                    double lateral,
                                                                                    double yaw,
                                                                                    double height,
                                                                                    long long timeMs,
                                                                                    double lift) const {
    // Mode marche tripod: deux groupes de pattes en opposition de phase.
    // Les gains sont differents du mode statique pour favoriser un deplacement fluide.
    //
    // Rappel tripod:
    // - Tripod A: FL, MR, RL
    // - Tripod B: FR, ML, RR
    // Quand A est en support, B est en transfert, puis inversion.
    const int cycleMs = 700;
    const double stepSpan = 38.0;
    const double maxYawRad = 14.0 * kPi / 180.0;
    const double maxHeightShift = 24.0;
    const double maxSwingLift = 28.0;

    const double fwd = clamp(forward, -1.0, 1.0);
    const double lat = clamp(lateral, -1.0, 1.0);
    const double yw = clamp(yaw, -1.0, 1.0);
    const double h = clamp(height, -1.0, 1.0);
    const double extraLift = clamp(lift, 0.0, 1.0) * 14.0;

    // Phase globale du cycle [0,1).
    // 0.0 = debut cycle, 0.5 = inversion des tripodes.
    const double basePh = phase(timeMs, cycleMs);
    std::array<int, NUM_SERVOS> out{};
    out.fill(90);

    for (int legId = 0; legId < NUM_LEGS; ++legId) {
        // Tripod B = FR, ML, RR. Les autres pattes appartiennent au tripod A.
        const bool tripodB = (legId == 3 || legId == 1 || legId == 5);
        double legPh = basePh + (tripodB ? 0.5 : 0.0);
        if (legPh >= 1.0) {
            legPh -= 1.0;
        }

        // phaseGain pilote le glissement horizontal du pied.
        // swingZ ajoute la levee verticale pendant la phase aerienne.
        // Support  : pied "colle" au sol, pousse le robot.
        // Transfert: pied decolle, revient vers l'avant, puis repose.
        double phaseGain = 0.0;
        double swingZ = 0.0;
        if (legPh < 0.5) {
            // Support: le pied reste au sol et pousse.
            const double u = legPh / 0.5;
            phaseGain = supportCurve(u);
        } else {
            // Transfert: retour en avant avec levee sinusoidale.
            const double u = (legPh - 0.5) / 0.5;
            phaseGain = transferCurve(u);
            swingZ = std::sin(kPi * u) * maxSwingLift;
        }

        const double baseX = legAnchors_[legId][0];
        const double baseY = legAnchors_[legId][1];
        const double nfx = neutralFootLocal_[legId][0];
        const double nfy = neutralFootLocal_[legId][1];
        const double nfz = neutralFootLocal_[legId][2];

        // Couplage yaw <-> position de hanche pour tourner en conservant la stabilite.
        // Plus la patte est eloignee du centre, plus l'effet yaw est marque.
        const double yawDx = -maxYawRad * yw * baseY;
        const double yawDy = maxYawRad * yw * baseX;

        const double tx = nfx + phaseGain * (fwd * stepSpan + yawDx);
        const double ty = nfy + phaseGain * (lat * stepSpan + yawDy);
        const double tz = nfz - (h * maxHeightShift) + swingZ + extraLift;

        // Resolution IK puis mapping servo (offset/sens/limites).
        const auto joint = solveLegIk(tx, ty, tz);
        const auto servo = jointToServo(legId, joint[0], joint[1], joint[2]);

        const int i = legId * SERVOS_PER_LEG;
        out[i] = servo[0];
        out[i + 1] = servo[1];
        out[i + 2] = servo[2];
    }

    // Tableau final de 18 angles pret a etre envoye a ServoController.
    // Ordre: [patte0 coxa,femur,tibia, patte1 coxa,femur,tibia, ..., patte5 ...]
    return out;
}

}  // namespace spiderbot
