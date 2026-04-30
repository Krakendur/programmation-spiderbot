#pragma once

namespace spiderbot {

enum class RobotAppMode {
    Spiderbot,
    Spiderkey,
    Unknown,
};

class DisplayManager {
public:
    DisplayManager(int mosiPin = 5,
                   int misoPin = 4,
                   int clkPin = 6,
                   int csPin = 19,
                   int dcPin = 11,
                   int rstPin = 10);

    bool initialize();
    void drawMenu(RobotAppMode selectedMode);
    bool handleMenuInput(bool upPressed, bool downPressed, bool selectPressed);
    RobotAppMode getSelectedMode() const;
    void drawConfirmationScreen(RobotAppMode mode);
    void shutdown();

private:
    bool initSpi();
    bool initIli9488();
    void clearScreen();
    void drawText(int x, int y, const char* text, bool highlight = false);

    int mosiPin_;
    int misoPin_;
    int clkPin_;
    int csPin_;
    int dcPin_;
    int rstPin_;

    RobotAppMode selectedMode_;
    bool initialized_;
};

}  // namespace spiderbot
