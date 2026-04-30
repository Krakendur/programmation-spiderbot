#include "display_manager.hpp"
#include "esp32_runtime_wiring.hpp"
#include "robot_controller.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
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

spiderbot::RobotAppMode runMenuSelection(spiderbot::DisplayManager& display,
                                         spiderbot::BluePadManager& bluepad) {
    std::cout << "[Main] Starting display menu selection" << std::endl;
    display.drawMenu(spiderbot::RobotAppMode::Spiderbot);

    spiderbot::RobotAppMode selectedMode = spiderbot::RobotAppMode::Spiderbot;
    bool selectionConfirmed = false;
    const auto menuStartTime = std::chrono::steady_clock::now();
    const auto menuTimeout = std::chrono::seconds(30);

    while (!selectionConfirmed) {
        const auto gamepadData = bluepad.update();

        if (gamepadData.has_value()) {
            bool upPressed = gamepadData->buttons.count("up") && gamepadData->buttons.at("up");
            bool downPressed = gamepadData->buttons.count("down") && gamepadData->buttons.at("down");
            bool selectPressed = gamepadData->buttons.count("a") && gamepadData->buttons.at("a");

            if (display.handleMenuInput(upPressed, downPressed, selectPressed)) {
                selectedMode = display.getSelectedMode();
                selectionConfirmed = true;
            }
        }

        if (std::chrono::steady_clock::now() - menuStartTime > menuTimeout) {
            std::cout << "[Main] Menu timeout - defaulting to Spiderbot" << std::endl;
            selectedMode = spiderbot::RobotAppMode::Spiderbot;
            selectionConfirmed = true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    return selectedMode;
}

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

int main() {
    const auto servoPins = getServoPins();
    std::cout << "Spider-Bot ESP32 C++ - Initialization" << std::endl;

    try {
        spiderbot::DisplayManager display(DisplayPins::MOSI, DisplayPins::MISO,
                                          DisplayPins::CLK, DisplayPins::CS,
                                          DisplayPins::DC, DisplayPins::RST);
        if (!display.initialize()) {
            std::cerr << "[ERROR] Failed to initialize display" << std::endl;
            return 1;
        }

        spiderbot::BluePadManager bluepad;
        spiderbot::RobotAppMode selectedMode = runMenuSelection(display, bluepad);

        spiderbot::RobotController controller(servoPins, selectedMode);
        spiderbot::configureEsp32RuntimeWiring(controller);
        return controller.run();
    } catch (const std::invalid_argument& e) {
        std::cerr << "[ERROR] " << e.what() << std::endl;
        return 1;
    }
}

#endif
