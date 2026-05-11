#pragma once

#include <cstdint>

#include "driver/spi_master.h"

namespace spiderbot {

using Color = uint16_t;  // RGB565

static constexpr Color kColorBlack  = 0x0000;
static constexpr Color kColorWhite  = 0xFFFF;
static constexpr Color kColorOrange = 0xFD20;
static constexpr Color kColorCyan   = 0x07FF;
static constexpr Color kColorGray   = 0x4208;
static constexpr Color kColorDkGray = 0x2104;
static constexpr Color kColorGreen  = 0x07E0;
static constexpr Color kColorNavy   = 0x000F;
static constexpr Color kColorRed    = 0xF800;

enum class RobotAppMode {
    Spiderbot,
    Spiderkey,
    Unknown,
};

class DisplayManager {
public:
    DisplayManager(int mosiPin = 5,
                   int misoPin = 4,
                   int clkPin  = 6,
                   int csPin   = 19,
                   int dcPin   = 11,
                   int rstPin  = 10);
    ~DisplayManager();

    bool initialize();
    RobotAppMode runMenuLoop();   // boucle bloquante : clavier UART → retourne le mode choisi
    void drawMenu(RobotAppMode selectedMode);
    bool handleMenuInput(bool upPressed, bool downPressed, bool selectPressed);
    RobotAppMode getSelectedMode() const;
    void drawConfirmationScreen(RobotAppMode mode);
    void shutdown();

private:
    bool initSpi();
    bool initIli9488();

    void sendCmd(uint8_t cmd);
    void sendData(uint8_t data);
    void sendBuf(const uint8_t* buf, size_t len);

    void setWindow(int x0, int y0, int x1, int y1);
    void fillRect(int x, int y, int w, int h, Color color);
    void drawHLine(int x, int y, int len, Color color);
    void drawRect(int x, int y, int w, int h, Color color);
    void clearScreen(Color color = kColorBlack);

    void drawChar(int x, int y, char c, Color fg, Color bg, int scale = 1);
    void drawText(int x, int y, const char* text, Color fg,
                  Color bg = kColorBlack, int scale = 1);
    int  textWidth(const char* text, int scale = 1) const;

    void drawHeader();
    void drawFooter();
    void drawItems();

    int mosiPin_, misoPin_, clkPin_, csPin_, dcPin_, rstPin_;
    spi_device_handle_t spiDev_{nullptr};
    RobotAppMode selectedMode_{RobotAppMode::Spiderbot};
    bool initialized_{false};
};

}  // namespace spiderbot
