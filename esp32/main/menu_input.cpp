#include "menu_input.hpp"

#include <cmath>

#include "esp_log.h"
#include "esp_timer.h"

namespace spiderbot {

static const char* TAG = "MenuInput";

static constexpr float    kDeadzone     = 0.05f;  // ±5 %
static constexpr float    kJoyThresh    = 0.50f;  // seuil de déclenchement direction
static constexpr uint32_t kDebouncMs   = 20;      // debounce boutons
static constexpr int      kAdcCenter   = 2048;    // mi-course 12-bit
static constexpr int      kAdcMax      = 2048;    // demi-plage pour normalisation

static uint32_t nowMs() {
    return static_cast<uint32_t>(esp_timer_get_time() / 1000LL);
}

// ── Init ──────────────────────────────────────────────────────────────────────
bool MenuInput::initialize() {
    // ADC oneshot unit 1 (GPIO0 = CH0, GPIO1 = CH1)
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id  = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    if (adc_oneshot_new_unit(&unit_cfg, &adcHandle_) != ESP_OK) {
        ESP_LOGE(TAG, "ADC unit init failed");
        return false;
    }

    adc_oneshot_chan_cfg_t ch_cfg = {
        .atten    = ADC_ATTEN_DB_12,       // 0 – 2.8 V (couvre VCC 3.3 V joystick)
        .bitwidth = ADC_BITWIDTH_DEFAULT,  // 12-bit
    };
    adc_oneshot_config_channel(adcHandle_, kChX, &ch_cfg);
    adc_oneshot_config_channel(adcHandle_, kChY, &ch_cfg);

    // GPIO boutons : input pull-up, actif bas
    gpio_config_t io = {};
    io.pin_bit_mask  = (1ULL << kBtnSelect) | (1ULL << kBtnBack);
    io.mode          = GPIO_MODE_INPUT;
    io.pull_up_en    = GPIO_PULLUP_ENABLE;
    io.pull_down_en  = GPIO_PULLDOWN_DISABLE;
    io.intr_type     = GPIO_INTR_DISABLE;
    gpio_config(&io);

    initialized_ = true;
    ESP_LOGI(TAG, "Joystick GPIO%d/GPIO%d | BTN_SEL GPIO%d | BTN_BACK GPIO%d",
             (int)kChX, (int)kChY, (int)kBtnSelect, (int)kBtnBack);
    return true;
}

// ── ADC → valeur normalisée −1.0 … +1.0 ──────────────────────────────────────
float MenuInput::readNorm(adc_channel_t ch) {
    int raw = kAdcCenter;
    adc_oneshot_read(adcHandle_, ch, &raw);
    float n = static_cast<float>(raw - kAdcCenter) / static_cast<float>(kAdcMax);
    if (n < -1.0f) n = -1.0f;
    if (n >  1.0f) n =  1.0f;
    if (fabsf(n) < kDeadzone) n = 0.0f;
    return n;
}

// ── Joystick : une seule impulsion par inclinaison (state-machine) ─────────────
MenuEvent MenuInput::pollJoystick() {
    float ny = readNorm(kChY);  // axe vertical

    switch (joyState_) {
        case JoyState::Center:
            if (ny >  kJoyThresh) { joyState_ = JoyState::UpHeld;   return MenuEvent::Up; }
            if (ny < -kJoyThresh) { joyState_ = JoyState::DownHeld; return MenuEvent::Down; }
            break;
        case JoyState::UpHeld:
            if (ny < kDeadzone)  joyState_ = JoyState::Center;
            break;
        case JoyState::DownHeld:
            if (ny > -kDeadzone) joyState_ = JoyState::Center;
            break;
    }
    return MenuEvent::None;
}

// ── Bouton : debounce 20 ms, un seul feu par pression ─────────────────────────
MenuEvent MenuInput::pollBtn(gpio_num_t pin, bool& pressed,
                              uint32_t& pressMs, bool& fired, MenuEvent evt) {
    bool isLow = (gpio_get_level(pin) == 0);  // actif bas
    uint32_t now = nowMs();

    if (isLow && !pressed) {
        pressed = true; pressMs = now; fired = false;
    } else if (!isLow) {
        pressed = false; fired = false;
    } else if (isLow && pressed && !fired && (now - pressMs >= kDebouncMs)) {
        fired = true;
        return evt;
    }
    return MenuEvent::None;
}

// ── Point d'entrée public ─────────────────────────────────────────────────────
MenuEvent MenuInput::poll() {
    if (!initialized_) return MenuEvent::None;

    // Priorité : BTN_SELECT > BTN_BACK > joystick
    MenuEvent e = pollBtn(kBtnSelect, btn1Pressed_, btn1Ms_, btn1Fired_, MenuEvent::Select);
    if (e != MenuEvent::None) return e;

    e = pollBtn(kBtnBack, btn2Pressed_, btn2Ms_, btn2Fired_, MenuEvent::Back);
    if (e != MenuEvent::None) return e;

    return pollJoystick();
}

}  // namespace spiderbot
