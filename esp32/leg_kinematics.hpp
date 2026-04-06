#pragma once

#include <array>

namespace spiderbot {

// Moteur de cinematique inverse pour hexapode (6 pattes x 3 articulations).
// Toutes les sorties sont des angles servo en degres.
class LegKinematics {
public:
    static constexpr int NUM_LEGS = 6;
    static constexpr int SERVOS_PER_LEG = 3;
    static constexpr int NUM_SERVOS = 18;

    LegKinematics();

    // Mode statique: calcule une posture globale sans cycle de marche.
    // Entrees normalisees:
    // - forward, lateral, yaw, height dans [-1, 1]
    // - lift dans [0, 1]
    std::array<int, NUM_SERVOS> computeServoAngles(double forward,
                                                   double lateral,
                                                   double yaw,
                                                   double height,
                                                   double lift) const;

    // Mode dynamique tripod.
    // timeMs sert a calculer la phase de cycle de marche.
    std::array<int, NUM_SERVOS> computeTripodServoAngles(double forward,
                                                         double lateral,
                                                         double yaw,
                                                         double height,
                                                         long long timeMs,
                                                         double lift = 0.0) const;

private:
    // Utilitaires numeriques.
    static double clamp(double value, double low, double high);
    static double phase(long long timeMs, int cycleMs);
    static double supportCurve(double u);
    static double transferCurve(double u);

    // IK 3DDL locale pour une patte (repere patte, en mm).
    std::array<double, 3> solveLegIk(double x, double y, double z) const;

    // Conversion espace articulaire -> angles servo physiques (avec calibration).
    std::array<int, 3> jointToServo(int legId,
                                    double coxaDeg,
                                    double femurDeg,
                                    double tibiaDeg) const;

    // Longueurs des segments (mm).
    double coxaLen_;
    double femurLen_;
    double tibiaLen_;

    // Position des ancrages de pattes dans le repere corps.
    std::array<std::array<double, 2>, NUM_LEGS> legAnchors_;

    // Position neutre du pied par patte dans son repere local.
    std::array<std::array<double, 3>, NUM_LEGS> neutralFootLocal_;

    // Calibration servos par patte: centres, signes, limites.
    std::array<std::array<int, 3>, NUM_LEGS> servoCenters_;
    std::array<std::array<int, 3>, NUM_LEGS> servoSigns_;
    std::array<std::array<std::array<int, 2>, 3>, NUM_LEGS> servoLimits_;
};

}  // namespace spiderbot
