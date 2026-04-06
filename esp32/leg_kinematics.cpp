#include "leg_kinematics.hpp"

#include <cmath>

namespace spiderbot {

namespace {

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

double LegKinematics::clamp(double value, double low, double high) {
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

double LegKinematics::phase(long long timeMs, int cycleMs) {
    if (cycleMs <= 0) {
        return 0.0;
    }
    const long long modulo = timeMs % cycleMs;
    return static_cast<double>(modulo) / static_cast<double>(cycleMs);
}

double LegKinematics::supportCurve(double u) {
    return 1.0 - 2.0 * u;
}

double LegKinematics::transferCurve(double u) {
    return -1.0 + 2.0 * u;
}

std::array<double, 3> LegKinematics::solveLegIk(double x, double y, double z) const {
    // Etape 1: orientation horizontale (coxa).
    const double coxaDeg = std::atan2(y, x) * 180.0 / kPi;

    // Etape 2: reduction au plan 2D femur/tibia.
    double horizontal = std::sqrt(x * x + y * y) - coxaLen_;
    if (horizontal < 1e-6) {
        horizontal = 1e-6;
    }

    // Etape 3: clamp de la distance cible a la zone atteignable de la patte.
    double d = std::sqrt(horizontal * horizontal + z * z);
    const double maxReach = femurLen_ + tibiaLen_ - 1e-6;
    const double minReach = std::abs(femurLen_ - tibiaLen_) + 1e-6;
    d = clamp(d, minReach, maxReach);

    const double alpha = std::atan2(z, horizontal);
    const double beta = safeAcos((femurLen_ * femurLen_ + d * d - tibiaLen_ * tibiaLen_) /
                                 (2.0 * femurLen_ * d));
    const double femurDeg = (alpha + beta) * 180.0 / kPi;

    const double kneeInternal =
        safeAcos((femurLen_ * femurLen_ + tibiaLen_ * tibiaLen_ - d * d) /
                 (2.0 * femurLen_ * tibiaLen_));
    const double tibiaDeg = (kPi - kneeInternal) * 180.0 / kPi;

    return {coxaDeg, femurDeg, tibiaDeg};
}

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

    for (int legId = 0; legId < NUM_LEGS; ++legId) {
        const double baseX = legAnchors_[legId][0];
        const double baseY = legAnchors_[legId][1];
        const double nfx = neutralFootLocal_[legId][0];
        const double nfy = neutralFootLocal_[legId][1];
        const double nfz = neutralFootLocal_[legId][2];

        const double yawDx = -yawRad * baseY;
        const double yawDy = yawRad * baseX;

        const double tx = nfx - dx - yawDx;
        const double ty = nfy - dy - yawDy;
        const double tz = nfz - dz + dzLift;

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

        const double yawDx = -maxYawRad * yw * baseY;
        const double yawDy = maxYawRad * yw * baseX;

        const double tx = nfx + phaseGain * (fwd * stepSpan + yawDx);
        const double ty = nfy + phaseGain * (lat * stepSpan + yawDy);
        const double tz = nfz - (h * maxHeightShift) + swingZ + extraLift;

        const auto joint = solveLegIk(tx, ty, tz);
        const auto servo = jointToServo(legId, joint[0], joint[1], joint[2]);

        const int i = legId * SERVOS_PER_LEG;
        out[i] = servo[0];
        out[i + 1] = servo[1];
        out[i + 2] = servo[2];
    }

    return out;
}

}  // namespace spiderbot
