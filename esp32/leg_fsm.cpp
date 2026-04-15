#include "leg_fsm.hpp"

namespace spiderbot {

LegFSM::LegFSM() : state_(LegPhaseState::Support) {}

void LegFSM::update(double normalizedPhase) {
    if (normalizedPhase < 0.5) {
        state_ = LegPhaseState::Support;
    } else {
        state_ = LegPhaseState::Transfer;
    }
}

LegPhaseState LegFSM::state() const {
    return state_;
}

bool LegFSM::isInSupport() const {
    return state_ == LegPhaseState::Support;
}

}  // namespace spiderbot
