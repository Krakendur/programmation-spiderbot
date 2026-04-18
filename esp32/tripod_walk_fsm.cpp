#include "tripod_walk_fsm.hpp"

#include <cmath>
#include <string>

namespace spiderbot {

TripodWalkFSM::TripodWalkFSM() : legFsms_(), walkEnabled_(false), cycleMs_(700) {}

double TripodWalkFSM::phase(long long timeMs, int cycleMs) {
    if (cycleMs <= 0) {
        return 0.0;
    }
    const long long modulo = timeMs % cycleMs;
    return static_cast<double>(modulo) / static_cast<double>(cycleMs);
}

double TripodWalkFSM::computeActivity(const GamepadData& gamepadData) {
    const auto getAxis = [&](const std::string& name) {
        const auto it = gamepadData.axes.find(name);
        return (it == gamepadData.axes.end()) ? 0.0 : it->second;
    };

    const double forward = -getAxis("ly");
    const double lateral = getAxis("lx");
    const double yaw = getAxis("rx");
    return std::abs(forward) + std::abs(lateral) + std::abs(yaw);
}

bool TripodWalkFSM::update(const GamepadData& gamepadData, long long timeMs) {
    walkEnabled_ = computeActivity(gamepadData) > kActivityThreshold;

    const double basePh = phase(timeMs, cycleMs_);
    for (int legId = 0; legId < NUM_LEGS; ++legId) {
        const bool tripodB = (legId == 3 || legId == 1 || legId == 5);
        double legPh = basePh + (tripodB ? 0.5 : 0.0);
        if (legPh >= 1.0) {
            legPh -= 1.0;
        }
        legFsms_[legId].update(legPh);
    }

    return walkEnabled_;
}

bool TripodWalkFSM::walkEnabled() const {
    return walkEnabled_;
}

const std::array<LegFSM, TripodWalkFSM::NUM_LEGS>& TripodWalkFSM::legFsms() const {
    return legFsms_;
}

}  // namespace spiderbot
