#include "display_manager.hpp"
#include "spider_logo.hpp"

#include <cmath>
#include <cstring>
#include <iostream>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace spiderbot {

static const char* TAG = "Display";

// ── Screen geometry (landscape: long axe = 480px) ─────────────────────────────
static constexpr int kW = 480;
static constexpr int kH = 320;

// ── Layout (portrait terminal_menu → landscape écran) ────────────────────────
//
//  y=  8  ┌──[logo 56px]── SpiderControleur ────┐  header box (66px)
//  y= 74  └─────────────────────────────────────┘
//  y= 76  ──────────────── sep ──────────────────
//  y= 80  │  >  Spiderbot                        │  item 0 (70px)
//  y=150  │     Spiderkey                        │  item 1 (70px)
//  y=220  ──────────────── sep ──────────────────
//  y=224  HAUT/BAS : naviguer ...                  footer hints
//  y=312  ┌─────────────────────────────────────┐  bottom border
//
static constexpr int kMargin    =  8;
static constexpr int kBoxHdrH   = 66;   // hauteur du cadre titre (logo 56px + marges)
static constexpr int kSep1Y     = kMargin + kBoxHdrH + 2;   // 76
static constexpr int kItemsTop  = kSep1Y + 4;               // 80
static constexpr int kSep2Y     = 220;
static constexpr int kNumItems  = 2;
static constexpr int kItemH     = (kSep2Y - kItemsTop) / kNumItems; // 70

static const char* kMenuLabels[kNumItems] = {"Spiderbot", "Spiderkey"};

// ── Position du logo ──────────────────────────────────────────────────────────
static constexpr int kLogoCx        = kMargin + 5 + kLogoSize / 2;  // 41 px (en-tête)
static constexpr int kLogoCy        = kMargin + kBoxHdrH / 2;       // 41 px (en-tête)
static constexpr int kLogoConfirmCx = kW / 2;                       // 240 px (confirmation)
static constexpr int kLogoConfirmCy = kLogoSize / 2 + kMargin + 4;  // 36 px (confirmation)

// ── Animation : 3° par frame, rafraîchi toutes les 50 ms ─────────────────────
static constexpr uint32_t kLogoFrameMs  = 50;
static constexpr int      kLogoStepDeg  = 3;

// ── Timeout menu ──────────────────────────────────────────────────────────────

// ── Font Adafruit 5×7, ASCII 0x20–0x7E ────────────────────────────────────────
static const uint8_t kFont[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, // ' '
    {0x00,0x00,0x5F,0x00,0x00}, // '!'
    {0x00,0x07,0x00,0x07,0x00}, // '"'
    {0x14,0x7F,0x14,0x7F,0x14}, // '#'
    {0x24,0x2A,0x7F,0x2A,0x12}, // '$'
    {0x23,0x13,0x08,0x64,0x62}, // '%'
    {0x36,0x49,0x55,0x22,0x50}, // '&'
    {0x00,0x05,0x03,0x00,0x00}, // '\''
    {0x00,0x1C,0x22,0x41,0x00}, // '('
    {0x00,0x41,0x22,0x1C,0x00}, // ')'
    {0x14,0x08,0x3E,0x08,0x14}, // '*'
    {0x08,0x08,0x3E,0x08,0x08}, // '+'
    {0x00,0x50,0x30,0x00,0x00}, // ','
    {0x08,0x08,0x08,0x08,0x08}, // '-'
    {0x00,0x60,0x60,0x00,0x00}, // '.'
    {0x20,0x10,0x08,0x04,0x02}, // '/'
    {0x3E,0x51,0x49,0x45,0x3E}, // '0'
    {0x00,0x42,0x7F,0x40,0x00}, // '1'
    {0x42,0x61,0x51,0x49,0x46}, // '2'
    {0x21,0x41,0x45,0x4B,0x31}, // '3'
    {0x18,0x14,0x12,0x7F,0x10}, // '4'
    {0x27,0x45,0x45,0x45,0x39}, // '5'
    {0x3C,0x4A,0x49,0x49,0x30}, // '6'
    {0x01,0x71,0x09,0x05,0x03}, // '7'
    {0x36,0x49,0x49,0x49,0x36}, // '8'
    {0x06,0x49,0x49,0x29,0x1E}, // '9'
    {0x00,0x36,0x36,0x00,0x00}, // ':'
    {0x00,0x56,0x36,0x00,0x00}, // ';'
    {0x08,0x14,0x22,0x41,0x00}, // '<'
    {0x14,0x14,0x14,0x14,0x14}, // '='
    {0x00,0x41,0x22,0x14,0x08}, // '>'
    {0x02,0x01,0x51,0x09,0x06}, // '?'
    {0x32,0x49,0x79,0x41,0x3E}, // '@'
    {0x7E,0x11,0x11,0x11,0x7E}, // 'A'
    {0x7F,0x49,0x49,0x49,0x36}, // 'B'
    {0x3E,0x41,0x41,0x41,0x22}, // 'C'
    {0x7F,0x41,0x41,0x22,0x1C}, // 'D'
    {0x7F,0x49,0x49,0x49,0x41}, // 'E'
    {0x7F,0x09,0x09,0x09,0x01}, // 'F'
    {0x3E,0x41,0x49,0x49,0x7A}, // 'G'
    {0x7F,0x08,0x08,0x08,0x7F}, // 'H'
    {0x00,0x41,0x7F,0x41,0x00}, // 'I'
    {0x20,0x40,0x41,0x3F,0x01}, // 'J'
    {0x7F,0x08,0x14,0x22,0x41}, // 'K'
    {0x7F,0x40,0x40,0x40,0x40}, // 'L'
    {0x7F,0x02,0x0C,0x02,0x7F}, // 'M'
    {0x7F,0x04,0x08,0x10,0x7F}, // 'N'
    {0x3E,0x41,0x41,0x41,0x3E}, // 'O'
    {0x7F,0x09,0x09,0x09,0x06}, // 'P'
    {0x3E,0x41,0x51,0x21,0x5E}, // 'Q'
    {0x7F,0x09,0x19,0x29,0x46}, // 'R'
    {0x46,0x49,0x49,0x49,0x31}, // 'S'
    {0x01,0x01,0x7F,0x01,0x01}, // 'T'
    {0x3F,0x40,0x40,0x40,0x3F}, // 'U'
    {0x1F,0x20,0x40,0x20,0x1F}, // 'V'
    {0x3F,0x40,0x38,0x40,0x3F}, // 'W'
    {0x63,0x14,0x08,0x14,0x63}, // 'X'
    {0x07,0x08,0x70,0x08,0x07}, // 'Y'
    {0x61,0x51,0x49,0x45,0x43}, // 'Z'
    {0x00,0x7F,0x41,0x41,0x00}, // '['
    {0x02,0x04,0x08,0x10,0x20}, // '\'
    {0x00,0x41,0x41,0x7F,0x00}, // ']'
    {0x04,0x02,0x01,0x02,0x04}, // '^'
    {0x40,0x40,0x40,0x40,0x40}, // '_'
    {0x00,0x01,0x02,0x04,0x00}, // '`'
    {0x20,0x54,0x54,0x54,0x78}, // 'a'
    {0x7F,0x48,0x44,0x44,0x38}, // 'b'
    {0x38,0x44,0x44,0x44,0x20}, // 'c'
    {0x38,0x44,0x44,0x48,0x7F}, // 'd'
    {0x38,0x54,0x54,0x54,0x18}, // 'e'
    {0x08,0x7E,0x09,0x01,0x02}, // 'f'
    {0x0C,0x52,0x52,0x52,0x3E}, // 'g'
    {0x7F,0x08,0x04,0x04,0x78}, // 'h'
    {0x00,0x44,0x7D,0x40,0x00}, // 'i'
    {0x20,0x40,0x44,0x3D,0x00}, // 'j'
    {0x7F,0x10,0x28,0x44,0x00}, // 'k'
    {0x00,0x41,0x7F,0x40,0x00}, // 'l'
    {0x7C,0x04,0x18,0x04,0x78}, // 'm'
    {0x7C,0x08,0x04,0x04,0x78}, // 'n'
    {0x38,0x44,0x44,0x44,0x38}, // 'o'
    {0x7C,0x14,0x14,0x14,0x08}, // 'p'
    {0x08,0x14,0x14,0x18,0x7C}, // 'q'
    {0x7C,0x08,0x04,0x04,0x08}, // 'r'
    {0x48,0x54,0x54,0x54,0x20}, // 's'
    {0x04,0x3F,0x44,0x40,0x20}, // 't'
    {0x3C,0x40,0x40,0x40,0x7C}, // 'u'
    {0x1C,0x20,0x40,0x20,0x1C}, // 'v'
    {0x3C,0x40,0x30,0x40,0x3C}, // 'w'
    {0x44,0x28,0x10,0x28,0x44}, // 'x'
    {0x0C,0x50,0x50,0x50,0x3C}, // 'y'
    {0x44,0x64,0x54,0x4C,0x44}, // 'z'
    {0x00,0x08,0x36,0x41,0x00}, // '{'
    {0x00,0x00,0x7F,0x00,0x00}, // '|'
    {0x00,0x41,0x36,0x08,0x00}, // '}'
    {0x10,0x08,0x08,0x10,0x08}, // '~'
};

// ── RGB565 → ILI9488 18-bit (3 octets) ───────────────────────────────────────
static inline void color18(Color c, uint8_t& r, uint8_t& g, uint8_t& b) {
    r = (c & 0xF800) >> 8;
    g = (c & 0x07E0) >> 3;
    b = (c & 0x001F) << 3;
}

// ── Ctor / Dtor ───────────────────────────────────────────────────────────────
DisplayManager::DisplayManager(int mosiPin, int misoPin, int clkPin,
                               int csPin, int dcPin, int rstPin)
    : mosiPin_(mosiPin), misoPin_(misoPin), clkPin_(clkPin),
      csPin_(csPin), dcPin_(dcPin), rstPin_(rstPin) {}

DisplayManager::~DisplayManager() { shutdown(); }

// ── SPI helpers ───────────────────────────────────────────────────────────────
void DisplayManager::sendCmd(uint8_t cmd) {
    gpio_set_level(static_cast<gpio_num_t>(dcPin_), 0);
    spi_transaction_t t{};
    t.length = 8; t.tx_data[0] = cmd; t.flags = SPI_TRANS_USE_TXDATA;
    spi_device_polling_transmit(spiDev_, &t);
}

void DisplayManager::sendData(uint8_t data) {
    gpio_set_level(static_cast<gpio_num_t>(dcPin_), 1);
    spi_transaction_t t{};
    t.length = 8; t.tx_data[0] = data; t.flags = SPI_TRANS_USE_TXDATA;
    spi_device_polling_transmit(spiDev_, &t);
}

void DisplayManager::sendBuf(const uint8_t* buf, size_t len) {
    if (!len) return;
    gpio_set_level(static_cast<gpio_num_t>(dcPin_), 1);
    spi_transaction_t t{};
    t.length = len * 8; t.tx_buffer = buf;
    spi_device_polling_transmit(spiDev_, &t);
}

// ── Primitives ────────────────────────────────────────────────────────────────
void DisplayManager::setWindow(int x0, int y0, int x1, int y1) {
    sendCmd(0x2A);
    sendData(x0 >> 8); sendData(x0 & 0xFF);
    sendData(x1 >> 8); sendData(x1 & 0xFF);
    sendCmd(0x2B);
    sendData(y0 >> 8); sendData(y0 & 0xFF);
    sendData(y1 >> 8); sendData(y1 & 0xFF);
    sendCmd(0x2C);
}

void DisplayManager::fillRect(int x, int y, int w, int h, Color color) {
    if (w <= 0 || h <= 0) return;
    setWindow(x, y, x + w - 1, y + h - 1);
    uint8_t r, g, b;
    color18(color, r, g, b);
    constexpr int kChunk = 64;
    uint8_t buf[kChunk * 3];
    for (int i = 0; i < kChunk; i++) { buf[i*3]=r; buf[i*3+1]=g; buf[i*3+2]=b; }
    for (int rem = w * h; rem > 0; rem -= kChunk) {
        sendBuf(buf, static_cast<size_t>((rem > kChunk ? kChunk : rem) * 3));
    }
}

void DisplayManager::drawHLine(int x, int y, int len, Color color) {
    fillRect(x, y, len, 1, color);
}

void DisplayManager::drawRect(int x, int y, int w, int h, Color color) {
    drawHLine(x,         y,         w, color);
    drawHLine(x,         y + h - 1, w, color);
    fillRect (x,         y + 1, 1, h - 2, color);
    fillRect (x + w - 1, y + 1, 1, h - 2, color);
}

void DisplayManager::clearScreen(Color color) {
    fillRect(0, 0, kW, kH, color);
}

// ── Texte ─────────────────────────────────────────────────────────────────────
// Cellule : 6×8 px (glyphe 5×7 + gap 1px) × scale.
void DisplayManager::drawChar(int x, int y, char c, Color fg, Color bg, int scale) {
    if (c < 0x20 || c > 0x7E) c = '?';
    const uint8_t* g = kFont[static_cast<uint8_t>(c) - 0x20];
    fillRect(x, y, 6 * scale, 8 * scale, bg);
    for (int col = 0; col < 5; col++) {
        uint8_t line = g[col];
        for (int row = 0; row < 7; row++) {
            if (line & (1 << row))
                fillRect(x + col * scale, y + row * scale, scale, scale, fg);
        }
    }
}

void DisplayManager::drawText(int x, int y, const char* text,
                               Color fg, Color bg, int scale) {
    for (int cx = x; *text; text++, cx += 6 * scale)
        drawChar(cx, y, *text, fg, bg, scale);
}

int DisplayManager::textWidth(const char* text, int scale) const {
    return static_cast<int>(strlen(text)) * 6 * scale;
}

// ── ILI9488 init ──────────────────────────────────────────────────────────────
bool DisplayManager::initSpi() {
    gpio_config_t io{};
    io.mode = GPIO_MODE_OUTPUT;
    io.pin_bit_mask = (1ULL << dcPin_) | (1ULL << rstPin_);
    gpio_config(&io);
    gpio_set_level(static_cast<gpio_num_t>(rstPin_), 1);

    spi_bus_config_t bus{};
    bus.mosi_io_num = mosiPin_; bus.miso_io_num = misoPin_;
    bus.sclk_io_num = clkPin_;
    bus.quadwp_io_num = -1;    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = kLogoSize * kLogoSize * 3;  // suffisant pour le sprite logo

    if (spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed"); return false;
    }
    spi_device_interface_config_t dev{};
    dev.clock_speed_hz = 40'000'000;
    dev.mode = 0; dev.spics_io_num = csPin_; dev.queue_size = 1;
    if (spi_bus_add_device(SPI2_HOST, &dev, &spiDev_) != ESP_OK) {
        ESP_LOGE(TAG, "SPI add device failed"); return false;
    }
    return true;
}

bool DisplayManager::initIli9488() {
    gpio_set_level(static_cast<gpio_num_t>(rstPin_), 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(static_cast<gpio_num_t>(rstPin_), 1);
    vTaskDelay(pdMS_TO_TICKS(150));

    sendCmd(0x01); vTaskDelay(pdMS_TO_TICKS(120));  // SW reset
    sendCmd(0x11); vTaskDelay(pdMS_TO_TICKS(120));  // Sleep out

    sendCmd(0x3A); sendData(0x66);  // 18-bit color

    // MADCTL: MV=1 (échange lignes/colonnes) → landscape 480×320, BGR=1
    sendCmd(0x36); sendData(0x28);

    sendCmd(0xC0); sendData(0x17); sendData(0x15);  // Power ctrl 1
    sendCmd(0xC1); sendData(0x41);                   // Power ctrl 2
    sendCmd(0xC5); sendData(0x00); sendData(0x12); sendData(0x80);
    sendCmd(0xB1); sendData(0xA0);                   // Frame rate
    sendCmd(0xB6); sendData(0x02); sendData(0x02); sendData(0x3B);
    sendCmd(0xB7); sendData(0xC6);                   // Entry mode

    sendCmd(0x29); vTaskDelay(pdMS_TO_TICKS(25));   // Display on
    return true;
}

// ── Logo rotatif ──────────────────────────────────────────────────────────────
void DisplayManager::drawRotatedLogo(int cx, int cy) {
    constexpr int S    = kLogoSize;
    constexpr int half = S / 2;

    float rad  = logoAngleDeg_ * (3.14159265f / 180.0f);
    float cosA = cosf(rad);
    float sinA = sinf(rad);

    // Buffer statique en DRAM (DMA-safe) : 32×32×3 = 3072 octets
    static uint8_t buf[S * S * 3];
    int idx = 0;

    for (int dy = 0; dy < S; dy++) {
        for (int dx = 0; dx < S; dx++) {
            // Rotation inverse : pixel destination → pixel source dans le sprite
            int px = dx - half;
            int py = dy - half;
            int sx = static_cast<int>(px * cosA + py * sinA) + half;
            int sy = static_cast<int>(-px * sinA + py * cosA) + half;

            Color c = kColorBlack;
            if (sx >= 0 && sx < S && sy >= 0 && sy < S) {
                Color raw = kSpiderLogoData[sy * S + sx];
                if (raw != 0xFFFF) c = raw;  // 0xFFFF = transparent → fond noir
            }
            uint8_t r, g, b;
            color18(c, r, g, b);
            buf[idx++] = r;
            buf[idx++] = g;
            buf[idx++] = b;
        }
    }

    setWindow(cx - half, cy - half, cx + half - 1, cy + half - 1);
    sendBuf(buf, sizeof(buf));
}

// ── Sections du menu (style terminal_menu) ────────────────────────────────────

void DisplayManager::drawHeader() {
    fillRect(kMargin + 1, kMargin + 1, kW - 2 * kMargin - 2, kBoxHdrH - 2, kColorBlack);
    drawRect(kMargin, kMargin, kW - 2 * kMargin, kBoxHdrH, kColorGray);

    drawRotatedLogo(kLogoCx, kLogoCy);

    // Titre à droite du logo, centré dans l'espace restant
    const char* title = "SpiderControleur";
    const int   scale = 2;
    const int   txStart = kLogoCx + kLogoSize / 2 + 10;
    const int   avail   = kW - kMargin - txStart;
    int tx = txStart + (avail - textWidth(title, scale)) / 2;
    int ty = kMargin + (kBoxHdrH - 8 * scale) / 2;
    drawText(tx, ty, title, kColorOrange, kColorBlack, scale);

    drawHLine(kMargin, kSep1Y, kW - 2 * kMargin, kColorGray);
}

void DisplayManager::drawFooter() {
    drawRect(kMargin, kSep2Y, kW - 2 * kMargin, kH - kSep2Y - kMargin, kColorGray);
}

void DisplayManager::drawItems() {
    for (int i = 0; i < kNumItems; i++) {
        int iy  = kItemsTop + i * kItemH;
        int ih  = kItemH - 2;
        bool sel = (i == static_cast<int>(selectedMode_));

        Color bg = sel ? kColorRed : kColorBlack;
        fillRect(kMargin + 1, iy, kW - 2 * kMargin - 2, ih, bg);

        // Curseur ">" (comme "  ▶ " dans terminal_menu)
        int tx = kMargin + 16;
        int ty = iy + (ih - 8 * 2) / 2;
        drawText(tx, ty, sel ? ">" : " ", sel ? kColorOrange : kColorBlack, bg, 2);

        // Label
        Color fg = sel ? kColorWhite : kColorGray;
        drawText(tx + 6 * 2 + 6, ty, kMenuLabels[i], fg, bg, 2);
    }
}

// ── Boucle menu : joystick + boutons physiques ET clavier UART ────────────────
RobotAppMode DisplayManager::runMenuLoop(MenuInput& input) {
    if (!uart_is_driver_installed(UART_NUM_0)) {
        uart_driver_install(UART_NUM_0, 256, 0, 0, nullptr, 0);
        uart_set_baudrate(UART_NUM_0, 115200);
        ESP_LOGI(TAG, "UART0 driver installed");
    }
    ESP_LOGI(TAG, "Menu loop - joystick/boutons + clavier UART");

    bool pendingConfirm = false;
    uint32_t lastLogoMs = static_cast<uint32_t>(esp_timer_get_time() / 1000LL);

    // Retour au menu depuis l'écran de confirmation
    auto goBack = [&]() {
        pendingConfirm = false;
        clearScreen();
        drawHeader(); drawItems(); drawFooter();
    };

    // Traitement d'un événement de navigation (commun UART et physique)
    auto dispatch = [&](MenuEvent evt) -> bool {
        switch (evt) {
            case MenuEvent::Up:
                if (!pendingConfirm) handleMenuInput(true, false, false);
                break;
            case MenuEvent::Down:
                if (!pendingConfirm) handleMenuInput(false, true, false);
                break;
            case MenuEvent::Select:
                if (!pendingConfirm) {
                    drawConfirmationScreen(selectedMode_);
                    pendingConfirm = true;
                } else {
                    return true;  // validation définitive → sortie boucle
                }
                break;
            case MenuEvent::Back:
                if (pendingConfirm) goBack();
                break;
            default:
                break;
        }
        return false;
    };

    while (true) {
        // ── Animation logo (menu ET écran de confirmation) ────────────────────
        {
            uint32_t nowMs = static_cast<uint32_t>(esp_timer_get_time() / 1000LL);
            if (nowMs - lastLogoMs >= kLogoFrameMs) {
                lastLogoMs = nowMs;
                logoAngleDeg_ = static_cast<uint16_t>((logoAngleDeg_ + kLogoStepDeg) % 360);
                if (pendingConfirm)
                    drawRotatedLogo(kLogoConfirmCx, kLogoConfirmCy);
                else
                    drawRotatedLogo(kLogoCx, kLogoCy);
            }
        }

        // ── Source 1 : entrées physiques (joystick + boutons) ─────────────────
        MenuEvent phys = input.poll();
        if (phys != MenuEvent::None && dispatch(phys)) return selectedMode_;

        // ── Source 2 : clavier UART (développement / debug) ──────────────────
        uint8_t c = 0;
        int len = uart_read_bytes(UART_NUM_0, &c, 1, pdMS_TO_TICKS(10));
        if (len > 0) {
            MenuEvent kbd = MenuEvent::None;
            if (c == 0x1B) {
                uint8_t seq[2] = {0, 0};
                uart_read_bytes(UART_NUM_0, &seq[0], 1, pdMS_TO_TICKS(50));
                if (seq[0] == '[') {
                    uart_read_bytes(UART_NUM_0, &seq[1], 1, pdMS_TO_TICKS(50));
                    if      (seq[1] == 'A') kbd = MenuEvent::Up;
                    else if (seq[1] == 'B') kbd = MenuEvent::Down;
                } else {
                    kbd = MenuEvent::Back;  // ESC seul
                }
            } else if (c == '\r' || c == '\n') {
                kbd = MenuEvent::Select;
            } else if (c == 0x7F || c == 0x08) {
                kbd = MenuEvent::Back;
            }
            if (kbd != MenuEvent::None && dispatch(kbd)) return selectedMode_;
        }
    }
}

// ── API publique ──────────────────────────────────────────────────────────────
bool DisplayManager::initialize() {
    if (!initSpi()) { ESP_LOGE(TAG, "SPI failed"); return false; }
    if (!initIli9488()) { ESP_LOGE(TAG, "ILI9488 failed"); return false; }
    initialized_ = true;
    clearScreen();
    drawHeader();
    drawItems();
    drawFooter();
    ESP_LOGI(TAG, "ILI9488 ready (landscape 480x320)");
    return true;
}

void DisplayManager::drawMenu(RobotAppMode mode) {
    if (!initialized_) return;
    selectedMode_ = mode;
    drawItems();
}

bool DisplayManager::handleMenuInput(bool upPressed, bool downPressed,
                                     bool selectPressed) {
    if (!initialized_) return false;
    if (upPressed) {
        int cur = static_cast<int>(selectedMode_);
        selectedMode_ = static_cast<RobotAppMode>((cur - 1 + kNumItems) % kNumItems);
        drawItems();
    } else if (downPressed) {
        int cur = static_cast<int>(selectedMode_);
        selectedMode_ = static_cast<RobotAppMode>((cur + 1) % kNumItems);
        drawItems();
    }
    if (selectPressed) {
        drawConfirmationScreen(selectedMode_);
        return true;
    }
    return false;
}

RobotAppMode DisplayManager::getSelectedMode() const { return selectedMode_; }

void DisplayManager::drawConfirmationScreen(RobotAppMode mode) {
    if (!initialized_) return;
    clearScreen();
    drawRect(kMargin, kMargin, kW - 2 * kMargin, kH - 2 * kMargin, kColorGray);

    // Logo centré en haut de l'écran
    drawRotatedLogo(kLogoConfirmCx, kLogoConfirmCy);

    // Séparateur sous le logo
    const int sepY = kLogoConfirmCy + kLogoSize / 2 + 8;
    drawHLine(kMargin, sepY, kW - 2 * kMargin, kColorGray);

    // Nom du mode sélectionné (grand, centré)
    const char* name = (mode == RobotAppMode::Spiderbot) ? "Spiderbot" : "Spiderkey";
    drawText((kW - textWidth(name, 3)) / 2, sepY + 20, name, kColorWhite, kColorBlack, 3);

    // Hints confirmer / annuler
    const char* confirm = "ENTREE : confirmer";
    const char* cancel  = "RETOUR : annuler";
    drawText((kW - textWidth(confirm, 1)) / 2, 230, confirm, kColorGreen,  kColorBlack, 1);
    drawText((kW - textWidth(cancel,  1)) / 2, 248, cancel,  kColorOrange, kColorBlack, 1);
}

void DisplayManager::showBleStatus(const char* status) {
    if (!initialized_) return;
    clearScreen();
    drawRect(kMargin, kMargin, kW - 2 * kMargin, kH - 2 * kMargin, kColorGray);

    drawRotatedLogo(kLogoConfirmCx, kLogoConfirmCy);

    const int sepY = kLogoConfirmCy + kLogoSize / 2 + 8;
    drawHLine(kMargin, sepY, kW - 2 * kMargin, kColorGray);

    drawText((kW - textWidth("Spiderbot", 3)) / 2, sepY + 20,
             "Spiderbot", kColorWhite, kColorBlack, 3);

    drawText((kW - textWidth(status, 2)) / 2, sepY + 80,
             status, kColorCyan, kColorBlack, 2);
}

void DisplayManager::tickBleLogo() {
    if (!initialized_) return;
    logoAngleDeg_ = static_cast<uint16_t>((logoAngleDeg_ + kLogoStepDeg) % 360);
    drawRotatedLogo(kLogoConfirmCx, kLogoConfirmCy);
}

void DisplayManager::shutdown() {
    if (!initialized_) return;
    clearScreen();
    if (spiDev_) { spi_bus_remove_device(spiDev_); spiDev_ = nullptr; }
    initialized_ = false;
}

}  // namespace spiderbot
