#include "ble_controller.hpp"
#include "display_manager.hpp"
#include "menu_input.hpp"

#include <iostream>

#include "esp_log.h"

static const char* TAG = "Main";

namespace {

struct DisplayPins {
    static constexpr int MOSI = 5;
    static constexpr int MISO = 4;
    static constexpr int CLK  = 6;
    static constexpr int CS   = 19;
    static constexpr int DC   = 11;
    static constexpr int RST  = 10;
};

}  // namespace

extern "C" void app_main() {
    spiderbot::DisplayManager display(DisplayPins::MOSI, DisplayPins::MISO,
                                      DisplayPins::CLK,  DisplayPins::CS,
                                      DisplayPins::DC,   DisplayPins::RST);
    if (!display.initialize()) {
        ESP_LOGE(TAG, "Display init failed");
        return;
    }

    spiderbot::MenuInput input;
    if (!input.initialize()) {
        ESP_LOGE(TAG, "Input init failed");
        return;
    }

    spiderbot::RobotAppMode mode = display.runMenuLoop(input);

    if (mode == spiderbot::RobotAppMode::Spiderbot) {
        spiderbot::BleController ble;
        if (!ble.initialize(display)) {
            ESP_LOGE(TAG, "BLE init failed");
            return;
        }
        ble.run(input);  // blocking — returns only on shutdown
    }

    // Spiderkey : TODO (future direct servo control via Spiderkey firmware)
    ESP_LOGI(TAG, "Mode %d selected — exiting app_main",
             static_cast<int>(mode));
}
