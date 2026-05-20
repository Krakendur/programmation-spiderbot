#pragma once

#include <cstdint>

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

namespace spiderbot {

enum class MenuEvent { None, Up, Down, Select, Back };

class MenuInput {
public:
    // ── Pinout joystick + boutons ──────────────────────────────────────────────
    static constexpr adc_channel_t kChX  = ADC_CHANNEL_0;  // GPIO0 - axe horizontal
    static constexpr adc_channel_t kChY  = ADC_CHANNEL_1;  // GPIO1 - axe vertical
    static constexpr gpio_num_t kBtnSelect = GPIO_NUM_2;    // BTN1 - validation
    static constexpr gpio_num_t kBtnBack   = GPIO_NUM_3;    // BTN2 - retour

    bool initialize();
    MenuEvent poll();  // non-bloquant, priorité : boutons > joystick

    // Lecture analogique directe pour le pilotage des servos en mode Spiderkey.
    float readAxisX();      // joystick horizontal [-1..1] (GPIO0)
    float readAxisY();      // joystick vertical   [-1..1] (GPIO1)
    bool  readBtnSelect();  // true si SELECT (GPIO2) pressé
    bool  readBtnBack();    // true si BACK   (GPIO3) pressé

private:
    enum class JoyState { Center, UpHeld, DownHeld };

    float       readNorm(adc_channel_t ch);
    MenuEvent   pollJoystick();
    MenuEvent   pollBtn(gpio_num_t pin, bool& pressed,
                        uint32_t& pressMs, bool& fired, MenuEvent evt);

    adc_oneshot_unit_handle_t adcHandle_{nullptr};
    JoyState joyState_{JoyState::Center};

    bool     btn1Pressed_{false};
    uint32_t btn1Ms_{0};
    bool     btn1Fired_{false};

    bool     btn2Pressed_{false};
    uint32_t btn2Ms_{0};
    bool     btn2Fired_{false};

    bool initialized_{false};
};

}  // namespace spiderbot
