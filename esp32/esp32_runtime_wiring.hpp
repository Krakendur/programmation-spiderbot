#pragma once

#include "robot_controller.hpp"

#include <array>

namespace spiderbot {

enum class Esp32WiringMode {
	Simulation,
	HybridValidation,
};

struct Esp32RuntimeWiringOptions {
	Esp32WiringMode mode;
	std::array<int, 6> validationServoIds;
};

// Mode hybride par defaut: valide les deux pattes avant completes.
inline constexpr std::array<int, 6> kDefaultHybridValidationServoIds{0, 1, 2, 9, 10, 11};

// Configure les ponts hardware optionnels (manette/PWM) pour une cible ESP32.
// En dehors d'un build embarque, cette fonction laisse un mode simulation non bloquant.
void configureEsp32RuntimeWiring(RobotController& controller);

// Version explicite avec options, utile pour les tests hybrides.
void configureEsp32RuntimeWiring(RobotController& controller,
								 const Esp32RuntimeWiringOptions& options);

}  // namespace spiderbot
