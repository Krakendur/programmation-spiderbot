// Point d'entree ESP-IDF pour le robot complet: DualSense + FSM + 18 servos.
// Architecture: btstack sur Core 0 (bloquant), robot FSM sur Core 1 (tache FreeRTOS).
// Les donnees gamepad transitent par une struct protegee par mutex FreeRTOS.

#include "bluepad_manager.hpp"
#include "robot_controller.hpp"
#include "servo_controller.hpp"

extern "C" {
#include "bt/uni_bt.h"
#include "btstack_port_esp32.h"
#include "btstack_run_loop.h"
#include "controller/uni_gamepad.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "platform/uni_platform.h"
#include "uni_hid_device.h"
#include "uni_init.h"
}

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// Etat partage entre la tache btstack (Core 0) et la tache robot (Core 1).
// Acces protege par gMutex.
// ---------------------------------------------------------------------------

struct SharedGamepad {
    uni_gamepad_t data{};
    bool connected = false;
    bool hasNew    = false;
};

SharedGamepad     gShared;
SemaphoreHandle_t gMutex = nullptr;

// ---------------------------------------------------------------------------
// Table de mapping servo_id -> canal LEDC.
// ESP32 dispose de 16 canaux: 8 LOW_SPEED (servos 0-7) + 8 HIGH_SPEED (8-15).
// Servos 16 et 17 n'ont pas de canal: le callback retourne true sans effet
// (no-op silencieux) pour eviter que la FSM parte en ERROR.
// ---------------------------------------------------------------------------

static constexpr int kNumServos = spiderbot::ServoController::NUM_SERVOS;

struct LedcMap {
    ledc_mode_t    mode;
    ledc_channel_t channel;
    bool           valid;
};

static const LedcMap kLedcMap[kNumServos] = {
    {LEDC_LOW_SPEED_MODE,  LEDC_CHANNEL_0, true},   // servo 0  GPIO 13
    {LEDC_LOW_SPEED_MODE,  LEDC_CHANNEL_1, true},   // servo 1  GPIO 14
    {LEDC_LOW_SPEED_MODE,  LEDC_CHANNEL_2, true},   // servo 2  GPIO 15
    {LEDC_LOW_SPEED_MODE,  LEDC_CHANNEL_3, true},   // servo 3  GPIO 16
    {LEDC_LOW_SPEED_MODE,  LEDC_CHANNEL_4, true},   // servo 4  GPIO 17
    {LEDC_LOW_SPEED_MODE,  LEDC_CHANNEL_5, true},   // servo 5  GPIO 18
    {LEDC_LOW_SPEED_MODE,  LEDC_CHANNEL_6, true},   // servo 6  GPIO 19
    {LEDC_LOW_SPEED_MODE,  LEDC_CHANNEL_7, true},   // servo 7  GPIO 21
    {LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, true},   // servo 8  GPIO 22
    {LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, true},   // servo 9  GPIO 23
    {LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_2, true},   // servo 10 GPIO 25
    {LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_3, true},   // servo 11 GPIO 26
    {LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_4, true},   // servo 12 GPIO 27
    {LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_5, true},   // servo 13 GPIO 32
    {LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_6, true},   // servo 14 GPIO 33
    {LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_7, true},   // servo 15 GPIO 4
    {LEDC_LOW_SPEED_MODE,  LEDC_CHANNEL_0, false},  // servo 16 GPIO 5  (no-op)
    {LEDC_LOW_SPEED_MODE,  LEDC_CHANNEL_0, false},  // servo 17 GPIO 2  (no-op)
};

static bool gChannelAttached[kNumServos]{};
static bool gTimerLowInited  = false;
static bool gTimerHighInited = false;

static uint32_t angleToLedcDuty(int angleDeg) {
    const int clamped = std::max(0, std::min(180, angleDeg));
    const int pulseUs = 500 + (2000 * clamped) / 180;
    return static_cast<uint32_t>(
        (static_cast<uint64_t>(pulseUs) * 65535u) / 20000u);
}

static bool initLedcTimerIfNeeded(ledc_mode_t mode) {
    bool& flag = (mode == LEDC_LOW_SPEED_MODE) ? gTimerLowInited : gTimerHighInited;
    if (flag) return true;

    ledc_timer_config_t timer = {};
    timer.speed_mode      = mode;
    timer.duty_resolution = LEDC_TIMER_16_BIT;
    timer.timer_num       = LEDC_TIMER_0;
    timer.freq_hz         = 50u;
    timer.clk_cfg         = LEDC_AUTO_CLK;
    const esp_err_t err = ledc_timer_config(&timer);
    if (err != ESP_OK) {
        printf("[SERVO] Erreur init timer LEDC mode=%d: %s\n",
               static_cast<int>(mode), esp_err_to_name(err));
        return false;
    }
    printf("[SERVO] LEDC timer init mode=%s\n",
           (mode == LEDC_LOW_SPEED_MODE) ? "LOW_SPEED" : "HIGH_SPEED");
    flag = true;
    return true;
}

static bool ledcWriteServo(int servoId, int gpioPin, int angleDeg) {
    if (servoId < 0 || servoId >= kNumServos) return false;
    const LedcMap& m = kLedcMap[servoId];
    if (!m.valid) return true;  // no-op silencieux pour servos 16-17

    if (!initLedcTimerIfNeeded(m.mode)) return false;

    if (!gChannelAttached[servoId]) {
        ledc_channel_config_t ch = {};
        ch.gpio_num   = gpioPin;
        ch.speed_mode = m.mode;
        ch.channel    = m.channel;
        ch.intr_type  = LEDC_INTR_DISABLE;
        ch.timer_sel  = LEDC_TIMER_0;
        ch.duty       = angleToLedcDuty(90);
        ch.hpoint     = 0;
        const esp_err_t err = ledc_channel_config(&ch);
        if (err != ESP_OK) {
            printf("[SERVO] Erreur canal servo %d GPIO %d: %s\n",
                   servoId, gpioPin, esp_err_to_name(err));
            return false;
        }
        gChannelAttached[servoId] = true;
        printf("[SERVO] servo %d GPIO %d pret\n", servoId, gpioPin);
    }

    ledc_set_duty(m.mode, m.channel, angleToLedcDuty(angleDeg));
    ledc_update_duty(m.mode, m.channel);
    return true;
}

static void ledcDetachServo(int servoId, int /*gpioPin*/) {
    if (servoId < 0 || servoId >= kNumServos) return;
    const LedcMap& m = kLedcMap[servoId];
    if (!m.valid || !gChannelAttached[servoId]) return;
    ledc_stop(m.mode, m.channel, 0);
    gChannelAttached[servoId] = false;
}

// ---------------------------------------------------------------------------
// Conversion uni_gamepad_t -> GamepadData (meme logique que fromBluepad()
// dans esp32_runtime_wiring.cpp, section SPIDERBOT_USE_BLUEPAD32_EXAMPLE).
// ---------------------------------------------------------------------------

static spiderbot::GamepadData fromUniGamepad(const uni_gamepad_t& gp) {
    spiderbot::GamepadData d;
    d.axes["lx"] = static_cast<double>(gp.axis_x)  / 512.0;
    d.axes["ly"] = static_cast<double>(gp.axis_y)  / 512.0;
    d.axes["rx"] = static_cast<double>(gp.axis_rx) / 512.0;
    d.axes["ry"] = static_cast<double>(gp.axis_ry) / 512.0;
    d.axes["lt"] = static_cast<double>(gp.brake)    / 1023.0;
    d.axes["rt"] = static_cast<double>(gp.throttle) / 1023.0;
    d.buttons["A"] = (gp.buttons & BUTTON_A) != 0;
    d.buttons["B"] = (gp.buttons & BUTTON_B) != 0;
    d.buttons["X"] = (gp.buttons & BUTTON_X) != 0;
    d.buttons["Y"] = (gp.buttons & BUTTON_Y) != 0;
    d.buttons["START"]  = (gp.misc_buttons & MISC_BUTTON_START) != 0;
    d.buttons["SELECT"] = (gp.misc_buttons & MISC_BUTTON_BACK)  != 0;
    return d;
}

// ---------------------------------------------------------------------------
// Callbacks plateforme btstack (meme pattern que bt_test_main.c).
// ---------------------------------------------------------------------------

static void cbInit(int, const char**) {
    printf("\n[ROBOT] Spider-Bot DualSense + 18 servos\n");
}

static void cbInitComplete() {
    printf("[ROBOT] Bluetooth pret - PS + Create pour appairer\n");
    uni_bt_start_scanning_and_autoconnect_unsafe();
}

static uni_error_t cbDiscovered(bd_addr_t, const char* name, uint16_t, uint8_t rssi) {
    printf("[ROBOT] Detecte: %s  RSSI=%d\n", name ? name : "?", rssi);
    return UNI_ERROR_SUCCESS;
}

static void cbDeviceConnected(uni_hid_device_t*) {
    printf("[ROBOT] Connexion HID...\n");
}

static void cbDeviceDisconnected(uni_hid_device_t*) {
    xSemaphoreTake(gMutex, portMAX_DELAY);
    gShared.connected = false;
    gShared.hasNew    = false;
    xSemaphoreGive(gMutex);
    printf("[ROBOT] Manette deconnectee\n");
}

static uni_error_t cbDeviceReady(uni_hid_device_t*) {
    xSemaphoreTake(gMutex, portMAX_DELAY);
    gShared.connected = true;
    xSemaphoreGive(gMutex);
    printf("[ROBOT] DualSense prete!\n");
    uni_bt_stop_scanning_unsafe();
    return UNI_ERROR_SUCCESS;
}

static void cbGamepadData(uni_hid_device_t*, uni_gamepad_t* gp) {
    xSemaphoreTake(gMutex, portMAX_DELAY);
    gShared.data   = *gp;
    gShared.hasNew = true;
    xSemaphoreGive(gMutex);
}

static const uni_property_t* cbGetProperty(uni_property_idx_t) { return nullptr; }
static void cbOobEvent(uni_platform_oob_event_t, void*) {}

static struct uni_platform sPlatform = {
    .name                   = "SpiderBot",
    .init                   = cbInit,
    .on_init_complete       = cbInitComplete,
    .on_device_discovered   = cbDiscovered,
    .on_device_connected    = cbDeviceConnected,
    .on_device_disconnected = cbDeviceDisconnected,
    .on_device_ready        = cbDeviceReady,
    .on_gamepad_data        = cbGamepadData,
    .on_controller_data     = nullptr,
    .get_property           = cbGetProperty,
    .on_oob_event           = cbOobEvent,
    .device_dump            = nullptr,
    .register_console_cmds  = nullptr,
};

// ---------------------------------------------------------------------------
// Tache robot (Core 1) — boucle 50 Hz bloquante.
// ---------------------------------------------------------------------------

static void robotTask(void*) {
    static const std::vector<int> kPins{
        13, 14, 15, 16, 17, 18,
        19, 21, 22, 23, 25, 26,
        27, 32, 33,  4,  5,  2
    };

    spiderbot::RobotController controller(kPins);

    controller.servoController().setPwmWriteCallback(
        [](int servoId, int gpioPin, int angleDeg) -> bool {
            return ledcWriteServo(servoId, gpioPin, angleDeg);
        });

    controller.servoController().setPwmDetachCallback(
        [](int servoId, int gpioPin) {
            ledcDetachServo(servoId, gpioPin);
        });

    controller.setTickCallback([&controller]() {
        xSemaphoreTake(gMutex, portMAX_DELAY);
        const bool connected  = gShared.connected;
        const bool hasNew     = gShared.hasNew;
        const uni_gamepad_t gp = gShared.data;
        gShared.hasNew = false;
        xSemaphoreGive(gMutex);

        if (!connected) {
            controller.bluepadManager().notifyDisconnected();
        } else if (hasNew) {
            controller.bluepadManager().publishFrame(fromUniGamepad(gp));
        }
    });

    controller.run();
    vTaskDelete(nullptr);
}

}  // namespace

// ---------------------------------------------------------------------------
// Entree principale ESP-IDF.
// ---------------------------------------------------------------------------

extern "C" void app_main() {
    gMutex = xSemaphoreCreateMutex();

    xTaskCreatePinnedToCore(robotTask, "robot", 16384, nullptr, 4, nullptr, 1);

    btstack_init();
    uni_platform_set_custom(&sPlatform);
    uni_init(0, nullptr);
    btstack_run_loop_execute();
}
