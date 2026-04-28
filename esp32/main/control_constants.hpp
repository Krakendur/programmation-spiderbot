#pragma once

namespace spiderbot {

// Constantes de controle partagees entre les FSM ESP32.
// Elles restent volontairement explicites pour conserver le comportement actuel.
inline constexpr int kBluepadTimeoutMs = 500;
inline constexpr double kActivityThreshold = 0.12;
inline constexpr double kTurnThreshold = 0.20;

}  // namespace spiderbot