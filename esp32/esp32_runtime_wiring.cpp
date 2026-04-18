#include "esp32_runtime_wiring.hpp"

#include <cmath>
#include <iostream>

#if defined(ARDUINO_ARCH_ESP32) && defined(SPIDERBOT_USE_LEDC_EXAMPLE)
#include <Arduino.h>
#endif

#if defined(ARDUINO_ARCH_ESP32) && defined(SPIDERBOT_USE_BLUEPAD32_EXAMPLE) && __has_include(<Bluepad32.h>)
#include <Bluepad32.h>
#endif

namespace spiderbot {

namespace {

constexpr Esp32RuntimeWiringOptions kDefaultHybridOptions{
    Esp32WiringMode::HybridValidation,
    kDefaultHybridValidationServoIds,
};

bool isValidationServo(int servoId, const std::array<int, 6>& activeServoIds) {
    for (int activeServoId : activeServoIds) {
        if (activeServoId == servoId) {
            return true;
        }
    }
    return false;
}

GamepadData buildSimulationFrame(int step) {
    GamepadData data;

    // Sequence de test simple pour faire traverser la chaine complete:
    // 0..19  : repos
    // 20..59 : avance
    // 60..89 : rotation
    // 90..109: recentrage explicite
    // 110..139: marche faible
    // 140+   : repos
    if (step >= 20 && step < 60) {
        data.axes["ly"] = -0.7;
        data.axes["rt"] = 0.8;
    } else if (step >= 60 && step < 90) {
        data.axes["rx"] = 0.6;
        data.axes["rt"] = 0.6;
    } else if (step >= 90 && step < 110) {
        data.buttons["X"] = true;
    } else if (step >= 110 && step < 140) {
        data.axes["lx"] = 0.35;
        data.axes["ly"] = -0.25;
        data.axes["rt"] = 0.5;
    }

    return data;
}

const char* validationServoLabel(int servoId) {
    switch (servoId) {
        case 0:
            return "front-left coxa";
        case 1:
            return "front-left femur";
        case 2:
            return "front-left tibia";
        case 9:
            return "front-right coxa";
        case 10:
            return "front-right femur";
        case 11:
            return "front-right tibia";
        default:
            return "inactive";
    }
}

#if defined(ARDUINO_ARCH_ESP32) && defined(SPIDERBOT_USE_BLUEPAD32_EXAMPLE) && __has_include(<Bluepad32.h>)

RobotController* gController = nullptr;

GamepadData fromBluepad(GamepadPtr gp) {
    GamepadData data;
    data.axes["lx"] = static_cast<double>(gp->axisX()) / 512.0;
    data.axes["ly"] = static_cast<double>(gp->axisY()) / 512.0;
    data.axes["rx"] = static_cast<double>(gp->axisRX()) / 512.0;
    data.axes["ry"] = static_cast<double>(gp->axisRY()) / 512.0;

    const double brake = static_cast<double>(gp->brake()) / 1023.0;
    const double throttle = static_cast<double>(gp->throttle()) / 1023.0;
    data.axes["lt"] = brake;
    data.axes["rt"] = throttle;

    data.buttons["A"] = gp->a();
    data.buttons["B"] = gp->b();
    data.buttons["X"] = gp->x();
    data.buttons["Y"] = gp->y();
    data.buttons["START"] = gp->start();
    data.buttons["SELECT"] = gp->select();
    return data;
}

void onConnectedGamepad(GamepadPtr gp) {
    if (gController == nullptr || !gp) {
        return;
    }
    gController->bluepadManager().publishFrame(fromBluepad(gp));
}

void onDisconnectedGamepad(GamepadPtr gp) {
    (void)gp;
    if (gController == nullptr) {
        return;
    }
    gController->bluepadManager().notifyDisconnected();
}

#endif

#if defined(ARDUINO_ARCH_ESP32) && defined(SPIDERBOT_USE_LEDC_EXAMPLE)

constexpr int kLedcMaxChannels = 16;
constexpr int kServoPwmFreqHz = 50;
constexpr int kServoPwmResolutionBits = 16;
constexpr int kServoMinPulseUs = 500;
constexpr int kServoMaxPulseUs = 2500;

int angleToDuty(int angleDeg) {
    const int clamped = (angleDeg < 0) ? 0 : ((angleDeg > 180) ? 180 : angleDeg);
    const double pulseUs =
        kServoMinPulseUs + (static_cast<double>(clamped) / 180.0) * (kServoMaxPulseUs - kServoMinPulseUs);
    const double periodUs = 1000000.0 / static_cast<double>(kServoPwmFreqHz);
    const double maxDuty = static_cast<double>((1U << kServoPwmResolutionBits) - 1U);
    return static_cast<int>(std::round((pulseUs / periodUs) * maxDuty));
}

#endif

}  // namespace

void configureEsp32RuntimeWiring(RobotController& controller) {
    configureEsp32RuntimeWiring(controller, kDefaultHybridOptions);
}

void configureEsp32RuntimeWiring(RobotController& controller,
                                 const Esp32RuntimeWiringOptions& options) {
#if defined(ARDUINO_ARCH_ESP32) && defined(SPIDERBOT_USE_BLUEPAD32_EXAMPLE) && __has_include(<Bluepad32.h>)
    std::cout << "[ESP32] Bluepad32 example backend active" << std::endl;
    gController = &controller;
    BP32.setup(&onConnectedGamepad, &onDisconnectedGamepad);
    controller.setTickCallback([]() {
        BP32.update();
        for (auto gp : BP32.getGamepads()) {
            if (gp && gp->isConnected()) {
                gController->bluepadManager().publishFrame(fromBluepad(gp));
            }
        }
    });
#else
    std::cout << "[ESP32] Bluepad32 backend disabled (simulation input)" << std::endl;
    controller.setTickCallback([&controller]() {
        static int simulationStep = 0;
        controller.bluepadManager().publishFrame(buildSimulationFrame(simulationStep));
        ++simulationStep;
    });
#endif

#if defined(ARDUINO_ARCH_ESP32) && defined(SPIDERBOT_USE_LEDC_EXAMPLE)
    if (options.mode == Esp32WiringMode::HybridValidation) {
        std::cout << "[ESP32] LEDC hybrid validation backend active (6 servos)" << std::endl;
        for (int servoId : options.validationServoIds) {
            std::cout << "[ESP32]   - servo " << servoId << " : "
                      << validationServoLabel(servoId) << std::endl;
        }
    } else {
        std::cout << "[ESP32] LEDC backend active" << std::endl;
    }

    auto& servo = controller.servoController();
    servo.setPwmWriteCallback([options](int servoId, int gpioPin, int angleDeg) {
        // Exemple LEDC minimal. Attention: ESP32 LEDC natif limite a 16 channels.
        if (servoId < 0 || servoId >= kLedcMaxChannels) {
            // Retourner false fait remonter un defaut servo vers la FSM de securite.
            return false;
        }

        if (options.mode == Esp32WiringMode::HybridValidation &&
            !isValidationServo(servoId, options.validationServoIds)) {
            // Les servos non valides restent en simulation logique le temps du test hybride.
            return true;
        }

        const int channel = servoId;
        static bool channelInitialized[kLedcMaxChannels] = {false};
        if (!channelInitialized[channel]) {
            ledcSetup(channel, kServoPwmFreqHz, kServoPwmResolutionBits);
            ledcAttachPin(gpioPin, channel);
            channelInitialized[channel] = true;
        }

        const int duty = angleToDuty(angleDeg);
        ledcWrite(channel, duty);
        return true;
    });

    servo.setPwmDetachCallback([options](int servoId, int gpioPin) {
        if (servoId < 0 || servoId >= kLedcMaxChannels) {
            return;
        }
        if (options.mode == Esp32WiringMode::HybridValidation &&
            !isValidationServo(servoId, options.validationServoIds)) {
            return;
        }
        ledcDetachPin(gpioPin);
    });
#else
    // Fallback dev: garde le mode simulation pour permettre les tests de logique FSM/IK.
    std::cout << "[ESP32] Runtime wiring in simulation mode (no hardware backend)" << std::endl;
    controller.servoController().setPwmWriteCallback([](int, int, int) {
        return true;
    });
    controller.servoController().setPwmDetachCallback([](int, int) {});
    (void)options;
#endif
}

}  // namespace spiderbot
