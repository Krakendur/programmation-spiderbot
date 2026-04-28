#include "esp32_runtime_wiring.hpp"

#include <cstring>
#include <iostream>

#if defined(ARDUINO_ARCH_ESP32) && defined(SPIDERBOT_USE_LEDC_EXAMPLE)
#include <Arduino.h>
#if __has_include(<ESP32Servo.h>)
#include <ESP32Servo.h>
#define SPIDERBOT_HAS_ESP32SERVO 1
#endif
#endif

#if defined(ARDUINO_ARCH_ESP32) && defined(SPIDERBOT_USE_BLUEPAD32_EXAMPLE) && __has_include(<Bluepad32.h>)
#include <Bluepad32.h>
#endif

#ifndef SPIDERBOT_HAS_ESP32SERVO
#define SPIDERBOT_HAS_ESP32SERVO 0
#endif

namespace spiderbot {

namespace {

constexpr Esp32RuntimeWiringOptions kDefaultHybridOptions{
    Esp32WiringMode::HybridValidation,
    kDefaultHybridValidationServoIds,
};

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

const char* simulationPhaseLabel(int step) {
    if (step < 20) {
        return "REST";
    }
    if (step < 60) {
        return "FORWARD";
    }
    if (step < 90) {
        return "TURN";
    }
    if (step < 110) {
        return "CENTER";
    }
    if (step < 140) {
        return "WALK_LIGHT";
    }
    return "REST";
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

#if defined(ARDUINO_ARCH_ESP32) && defined(SPIDERBOT_USE_LEDC_EXAMPLE) && SPIDERBOT_HAS_ESP32SERVO

constexpr int kServoPwmFreqHz = 50;
constexpr int kServoMinPulseUs = 500;
constexpr int kServoMaxPulseUs = 2500;
std::array<Servo, ServoController::NUM_SERVOS> gServos;
std::array<bool, ServoController::NUM_SERVOS> gServoAttached{};

bool isValidationServo(int servoId, const std::array<int, 6>& activeServoIds) {
    for (int activeServoId : activeServoIds) {
        if (activeServoId == servoId) {
            return true;
        }
    }
    return false;
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

bool isServoIdValid(int servoId) {
    return servoId >= 0 && servoId < ServoController::NUM_SERVOS;
}

int clampServoAngle(int angleDeg) {
    if (angleDeg < 0) {
        return 0;
    }
    if (angleDeg > 180) {
        return 180;
    }
    return angleDeg;
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
        static const char* lastPhase = nullptr;
        const char* phase = simulationPhaseLabel(simulationStep);
        if (lastPhase == nullptr || std::strcmp(phase, lastPhase) != 0) {
            std::cout << "[SIM] phase=" << phase << " step=" << simulationStep << std::endl;
            lastPhase = phase;
        }
        controller.bluepadManager().publishFrame(buildSimulationFrame(simulationStep));
        ++simulationStep;
    });
#endif

#if defined(ARDUINO_ARCH_ESP32) && defined(SPIDERBOT_USE_LEDC_EXAMPLE) && SPIDERBOT_HAS_ESP32SERVO
    if (options.mode == Esp32WiringMode::HybridValidation) {
        std::cout << "[ESP32] ESP32Servo hybrid validation backend active (6 servos)" << std::endl;
        for (int servoId : options.validationServoIds) {
            std::cout << "[ESP32]   - servo " << servoId << " : "
                      << validationServoLabel(servoId) << std::endl;
        }
    } else {
        std::cout << "[ESP32] ESP32Servo backend active" << std::endl;
    }

    auto& servo = controller.servoController();
    servo.setPwmWriteCallback([options](int servoId, int gpioPin, int angleDeg) {
        if (!isServoIdValid(servoId)) {
            // Retourner false fait remonter un defaut servo vers la FSM de securite.
            return false;
        }

        if (options.mode == Esp32WiringMode::HybridValidation &&
            !isValidationServo(servoId, options.validationServoIds)) {
            // Les servos non valides restent en simulation logique le temps du test hybride.
            return true;
        }

        if (!gServoAttached[servoId]) {
            gServos[servoId].setPeriodHertz(kServoPwmFreqHz);
            gServos[servoId].attach(gpioPin, kServoMinPulseUs, kServoMaxPulseUs);
            gServoAttached[servoId] = true;
        }

        gServos[servoId].write(clampServoAngle(angleDeg));
        return true;
    });

    servo.setPwmDetachCallback([options](int servoId, int gpioPin) {
        (void)gpioPin;
        if (!isServoIdValid(servoId)) {
            return;
        }
        if (options.mode == Esp32WiringMode::HybridValidation &&
            !isValidationServo(servoId, options.validationServoIds)) {
            return;
        }
        if (gServoAttached[servoId]) {
            gServos[servoId].detach();
            gServoAttached[servoId] = false;
        }
    });
#elif defined(ARDUINO_ARCH_ESP32) && defined(SPIDERBOT_USE_LEDC_EXAMPLE)
    std::cout << "[ESP32] ESP32Servo library missing, runtime wiring stays in simulation mode" << std::endl;
    controller.servoController().setPwmWriteCallback([](int, int, int) {
        return true;
    });
    controller.servoController().setPwmDetachCallback([](int, int) {});
    (void)options;
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
