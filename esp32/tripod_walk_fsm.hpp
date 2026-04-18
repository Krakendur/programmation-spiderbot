#pragma once

#include "bluepad_manager.hpp"
#include "leg_fsm.hpp"

#include <array>

namespace spiderbot {

// Locomotion FSM dedicated to tripod walk supervision.
// It decides whether walking is active and updates the 6 leg FSM phases.
class TripodWalkFSM {
public:
    static constexpr int NUM_LEGS = 6;

    TripodWalkFSM();

    bool update(const GamepadData& gamepadData, long long timeMs);
    bool walkEnabled() const;
    const std::array<LegFSM, NUM_LEGS>& legFsms() const;

private:
    static double phase(long long timeMs, int cycleMs);
    static double computeActivity(const GamepadData& gamepadData);

    std::array<LegFSM, NUM_LEGS> legFsms_;
    bool walkEnabled_;
    int cycleMs_;
    double activityThreshold_;
};

}  // namespace spiderbot
