#pragma once

namespace spiderbot {

enum class LegPhaseState {
    Support,
    Transfer,
};

// Per-leg FSM used by locomotion supervision.
class LegFSM {
public:
    LegFSM();

    // Normalized phase in [0, 1).
    void update(double normalizedPhase);

    LegPhaseState state() const;
    bool isInSupport() const;

private:
    LegPhaseState state_;
};

}  // namespace spiderbot
