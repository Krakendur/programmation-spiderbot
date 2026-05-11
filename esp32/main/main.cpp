#include "display_manager.hpp"
#include "esp32_runtime_wiring.hpp"
#include "robot_controller.hpp"

#include <iostream>
#include <memory>
#include <vector>

namespace {

std::vector<int> getServoPins() {
    // Mapping logique servo_id -> GPIO selon le tableau du README.
    return {13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33, 4, 5, 2};
}

// ILI9488 Display pin configuration
struct DisplayPins {
    static constexpr int MOSI = 5;
    static constexpr int MISO = 4;
    static constexpr int CLK = 6;
    static constexpr int CS = 19;
    static constexpr int DC = 11;
    static constexpr int RST = 10;
    // BL (Backlight) = 3.3V (fixed)
};

std::unique_ptr<spiderbot::RobotController> gController;

}  // namespace

#if defined(ARDUINO_ARCH_ESP32)

void setup() {
    const auto servoPins = getServoPins();
    std::cout << "Spider-Bot ESP32 C++ - Initialization" << std::endl;

    try {
        spiderbot::DisplayManager display(DisplayPins::MOSI, DisplayPins::MISO,
                                          DisplayPins::CLK, DisplayPins::CS,
                                          DisplayPins::DC, DisplayPins::RST);
        if (!display.initialize()) {
            std::cerr << "[ERROR] Failed to initialize display" << std::endl;
            return;
        }

        spiderbot::BluePadManager bluepad;
        spiderbot::RobotAppMode selectedMode = runMenuSelection(display, bluepad);

        gController = std::make_unique<spiderbot::RobotController>(servoPins, selectedMode);
        spiderbot::configureEsp32RuntimeWiring(*gController);
        gController->run();
    } catch (const std::invalid_argument& e) {
        std::cerr << "[ERROR] " << e.what() << std::endl;
    }
}

void loop() {
    // La logique tourne deja dans RobotController::run().
}

#else

// Point d'entree ESP-IDF: app_main() est appele par la tache principale FreeRTOS.
extern "C" void app_main() {
    const auto servoPins = getServoPins();
    std::cout << "Spider-Bot ESP32 C++ - Initialization" << std::endl;

    spiderbot::DisplayManager display(DisplayPins::MOSI, DisplayPins::MISO,
                                      DisplayPins::CLK, DisplayPins::CS,
                                      DisplayPins::DC, DisplayPins::RST);
    if (!display.initialize()) {
        std::cerr << "[ERROR] Failed to initialize display" << std::endl;
        return;
    }

    spiderbot::RobotAppMode selectedMode = display.runMenuLoop();

    spiderbot::RobotController controller(servoPins, selectedMode);
    spiderbot::configureEsp32RuntimeWiring(controller);
    controller.run();
}

#endif
