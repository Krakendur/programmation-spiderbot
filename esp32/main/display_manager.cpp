#include "display_manager.hpp"

#include <iostream>

namespace spiderbot {

DisplayManager::DisplayManager(int mosiPin,
                               int misoPin,
                               int clkPin,
                               int csPin,
                               int dcPin,
                               int rstPin)
    : mosiPin_(mosiPin),
      misoPin_(misoPin),
      clkPin_(clkPin),
      csPin_(csPin),
      dcPin_(dcPin),
      rstPin_(rstPin),
      selectedMode_(RobotAppMode::Spiderbot),
      initialized_(false) {}

bool DisplayManager::initialize() {
    if (!initSpi()) {
        std::cerr << "[Display] Failed to initialize SPI bus" << std::endl;
        return false;
    }

    if (!initIli9488()) {
        std::cerr << "[Display] Failed to initialize ILI9488 controller" << std::endl;
        return false;
    }

    initialized_ = true;
    clearScreen();
    drawMenu(selectedMode_);
    std::cout << "[Display] ILI9488 initialized" << std::endl;
    return true;
}

bool DisplayManager::initSpi() {
    std::cout << "[Display] SPI init MOSI=" << mosiPin_ << " MISO=" << misoPin_
              << " CLK=" << clkPin_ << " CS=" << csPin_ << std::endl;
    return true;
}

bool DisplayManager::initIli9488() {
    std::cout << "[Display] ILI9488 init DC=" << dcPin_ << " RST=" << rstPin_
              << std::endl;
    return true;
}

void DisplayManager::drawMenu(RobotAppMode selectedMode) {
    if (!initialized_) {
        return;
    }

    selectedMode_ = selectedMode;
    clearScreen();
    drawText(60, 40, "SELECT MODE");
    drawText(80, 120, "Spiderbot", selectedMode_ == RobotAppMode::Spiderbot);
    drawText(80, 180, "Spiderkey", selectedMode_ == RobotAppMode::Spiderkey);
    drawText(30, 280, "Use Buttons Up/Down", false);
    drawText(50, 300, "Press Select to confirm", false);
}

bool DisplayManager::handleMenuInput(bool upPressed,
                                     bool downPressed,
                                     bool selectPressed) {
    if (!initialized_) {
        return false;
    }

    if (upPressed && selectedMode_ == RobotAppMode::Spiderkey) {
        selectedMode_ = RobotAppMode::Spiderbot;
        drawMenu(selectedMode_);
    } else if (downPressed && selectedMode_ == RobotAppMode::Spiderbot) {
        selectedMode_ = RobotAppMode::Spiderkey;
        drawMenu(selectedMode_);
    }

    if (selectPressed) {
        drawConfirmationScreen(selectedMode_);
        return true;
    }

    return false;
}

RobotAppMode DisplayManager::getSelectedMode() const {
    return selectedMode_;
}

void DisplayManager::drawConfirmationScreen(RobotAppMode mode) {
    if (!initialized_) {
        return;
    }

    clearScreen();
    const char* modeName = (mode == RobotAppMode::Spiderbot) ? "SPIDERBOT" : "SPIDERKEY";
    drawText(60, 140, modeName);
    drawText(40, 180, "Starting...", false);
}

void DisplayManager::shutdown() {
    if (!initialized_) {
        return;
    }
    clearScreen();
    initialized_ = false;
}

void DisplayManager::clearScreen() {
    // Stub: clear screen content. TODO: replace with SPI commands.
}

void DisplayManager::drawText(int x, int y, const char* text, bool highlight) {
    if (!initialized_) {
        return;
    }

    if (highlight) {
        std::cout << "[Display] " << text << " (highlighted)" << std::endl;
    } else {
        std::cout << "[Display] " << text << std::endl;
    }
}

}  // namespace spiderbot
