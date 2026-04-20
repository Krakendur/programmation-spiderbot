#include "esp32_runtime_wiring.hpp"
#include "robot_controller.hpp"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

std::vector<int> getServoPins() {
    // Mapping logique servo_id -> GPIO. A adapter au cablage reel.
    return {2, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32};
}

std::unique_ptr<spiderbot::RobotController> gController;

}  // namespace

#if defined(ARDUINO_ARCH_ESP32)

void setup() {
    const auto servoPins = getServoPins();
    std::cout << "Spider-Bot ESP32 C++ - Initialization" << std::endl;

    try {
        gController = std::make_unique<spiderbot::RobotController>(servoPins);
        spiderbot::configureEsp32RuntimeWiring(*gController);
        // Boucle de controle principale (bloquante) sur cible embarquee.
        gController->run();
    } catch (const std::invalid_argument& e) {
        std::cerr << "[ERROR] " << e.what() << std::endl;
    }
}

void loop() {
    // La logique tourne deja dans RobotController::run().
}

#else

int main() {
    const auto servoPins = getServoPins();
    std::cout << "Spider-Bot ESP32 C++ - Initialization" << std::endl;

    try {
        spiderbot::RobotController controller(servoPins);
        spiderbot::configureEsp32RuntimeWiring(controller);
        return controller.run();
    } catch (const std::invalid_argument& e) {
        std::cerr << "[ERROR] " << e.what() << std::endl;
        return 1;
    }
}

#endif
