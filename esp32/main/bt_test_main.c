// Test materiel court: DualSense + 6 servos de validation.
// Compile uniquement avec le profil esp32dev-bt-test (SPIDERBOT_BT_TEST=1).
// Pas de FSM robot ici: ce fichier sert a valider BT + PWM avant integration complete.

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include <btstack_port_esp32.h>
#include <btstack_run_loop.h>

#include "bt/uni_bt.h"
#include "controller/uni_gamepad.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "platform/uni_platform.h"
#include "uni.h"
#include "uni_hid_device.h"

typedef struct {
    int servo_id;
    int gpio;
    ledc_channel_t channel;
    const char* label;
} validation_servo_t;

static const validation_servo_t validation_servos[] = {
    {0, 13, LEDC_CHANNEL_0, "front-left coxa"},
    {1, 14, LEDC_CHANNEL_1, "front-left femur"},
    {2, 15, LEDC_CHANNEL_2, "front-left tibia"},
    {9, 23, LEDC_CHANNEL_3, "front-right coxa"},
    {10, 25, LEDC_CHANNEL_4, "front-right femur"},
    {11, 26, LEDC_CHANNEL_5, "front-right tibia"},
};

enum {
    SERVO_COUNT = sizeof(validation_servos) / sizeof(validation_servos[0]),
    SERVO_CENTER_DEG = 90,
    SERVO_MIN_DEG = 60,
    SERVO_MAX_DEG = 120,
    SERVO_MIN_PULSE_US = 500,
    SERVO_MAX_PULSE_US = 2500,
    SERVO_PWM_FREQ_HZ = 50,
    SERVO_PWM_RES_BITS = LEDC_TIMER_16_BIT,
};

static bool joystick_origin_captured = false;
static int32_t joystick_origin_lx = 0;
static int32_t joystick_origin_ly = 0;
static int32_t joystick_origin_rx = 0;
static int32_t joystick_origin_ry = 0;
static bool servos_initialized = false;

static int32_t clamp_i32(int32_t value, int32_t min_value, int32_t max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static int clamp_angle(int angle_deg) {
    return (int)clamp_i32(angle_deg, SERVO_MIN_DEG, SERVO_MAX_DEG);
}

static uint32_t servo_angle_to_duty(int angle_deg) {
    const int clamped_angle = clamp_angle(angle_deg);
    const int pulse_span_us = SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US;
    const int pulse_us = SERVO_MIN_PULSE_US + (pulse_span_us * clamped_angle) / 180;
    const uint32_t max_duty = (1u << SERVO_PWM_RES_BITS) - 1u;
    return (uint32_t)((pulse_us * max_duty) / 20000u);
}

static void servo_write_angle(const validation_servo_t* servo, int angle_deg) {
    const uint32_t duty = servo_angle_to_duty(angle_deg);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, servo->channel, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, servo->channel);
}

static void center_validation_servos(void) {
    if (!servos_initialized) {
        return;
    }

    for (size_t i = 0; i < SERVO_COUNT; ++i) {
        servo_write_angle(&validation_servos[i], SERVO_CENTER_DEG);
    }
}

static void init_validation_servos(void) {
    if (servos_initialized) {
        return;
    }

    const ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = SERVO_PWM_RES_BITS,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = SERVO_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };

    esp_err_t err = ledc_timer_config(&timer_config);
    if (err != ESP_OK) {
        printf("[SERVO] Erreur init timer LEDC: %s\n", esp_err_to_name(err));
        return;
    }

    for (size_t i = 0; i < SERVO_COUNT; ++i) {
        const validation_servo_t* servo = &validation_servos[i];
        const ledc_channel_config_t channel_config = {
            .gpio_num = servo->gpio,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = servo->channel,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER_0,
            .duty = servo_angle_to_duty(SERVO_CENTER_DEG),
            .hpoint = 0,
            .flags = {.output_invert = 0},
        };

        err = ledc_channel_config(&channel_config);
        if (err != ESP_OK) {
            printf("[SERVO] Erreur init servo %d GPIO %d: %s\n",
                   servo->servo_id, servo->gpio, esp_err_to_name(err));
            return;
        }

        printf("[SERVO] servo %d GPIO %d pret (%s)\n",
               servo->servo_id, servo->gpio, servo->label);
    }

    servos_initialized = true;
    center_validation_servos();
    printf("[SERVO] 6 servos centres a %d degres\n", SERVO_CENTER_DEG);
}

static void apply_gamepad_to_servos(int32_t lx, int32_t ly, int32_t rx, int32_t ry,
                                    int32_t brake, int32_t throttle, uint16_t buttons) {
    if (!servos_initialized) {
        return;
    }

    if (buttons & BUTTON_X) {
        center_validation_servos();
        return;
    }

    const int coxa_delta = (int)((clamp_i32(lx, -512, 512) * 20) / 512);
    const int femur_delta = (int)((-clamp_i32(ly, -512, 512) * 20) / 512);
    const int yaw_delta = (int)((clamp_i32(rx, -512, 512) * 12) / 512);
    const int height_delta = (int)((-clamp_i32(ry, -512, 512) * 12) / 512);
    const int lift_delta = (int)((clamp_i32(throttle - brake, -1023, 1023) * 15) / 1023);

    servo_write_angle(&validation_servos[0], SERVO_CENTER_DEG + coxa_delta + yaw_delta);
    servo_write_angle(&validation_servos[1], SERVO_CENTER_DEG + femur_delta + height_delta + lift_delta);
    servo_write_angle(&validation_servos[2], SERVO_CENTER_DEG - femur_delta + lift_delta);
    servo_write_angle(&validation_servos[3], SERVO_CENTER_DEG + coxa_delta - yaw_delta);
    servo_write_angle(&validation_servos[4], SERVO_CENTER_DEG + femur_delta + height_delta + lift_delta);
    servo_write_angle(&validation_servos[5], SERVO_CENTER_DEG - femur_delta + lift_delta);
}

static void on_init(int argc, const char** argv) {
    (void)argc;
    (void)argv;
    printf("\n[BT] Spider-Bot Bluepad32 + 6 servos test demarre\n");
    init_validation_servos();
}

static void on_init_complete(void) {
    printf("[BT] Bluetooth pret\n");
    printf("[BT] --> Maintenez PS + Create sur la DualSense pour appairer\n");
    uni_bt_start_scanning_and_autoconnect_unsafe();
}

static uni_error_t on_device_discovered(bd_addr_t addr, const char* name, uint16_t cod, uint8_t rssi) {
    (void)addr;
    (void)cod;
    printf("[BT] Appareil detecte: %s  RSSI=%d\n", name ? name : "?", rssi);
    return UNI_ERROR_SUCCESS;
}

static void on_device_connected(uni_hid_device_t* d) {
    (void)d;
    printf("[BT] Connexion etablie, initialisation HID...\n");
}

static void on_device_disconnected(uni_hid_device_t* d) {
    (void)d;
    joystick_origin_captured = false;
    center_validation_servos();
    printf("[BT] Manette deconnectee\n");
}

static uni_error_t on_device_ready(uni_hid_device_t* d) {
    (void)d;
    printf("[BT] DualSense prete! Les entrees vont apparaitre ci-dessous.\n");
    uni_bt_stop_scanning_unsafe();
    printf("[BT] Scan arrete pendant le test d'une manette.\n");
    return UNI_ERROR_SUCCESS;
}

static void on_gamepad_data(uni_hid_device_t* d, uni_gamepad_t* gp) {
    (void)d;
    static int64_t last_log_us = 0;
    static uint32_t skipped_frames = 0;

    if (!joystick_origin_captured) {
        joystick_origin_lx = gp->axis_x;
        joystick_origin_ly = gp->axis_y;
        joystick_origin_rx = gp->axis_rx;
        joystick_origin_ry = gp->axis_ry;
        joystick_origin_captured = true;
        printf("[BT] Origine sticks capturee: lx=%ld ly=%ld rx=%ld ry=%ld\n",
               (long)joystick_origin_lx, (long)joystick_origin_ly,
               (long)joystick_origin_rx, (long)joystick_origin_ry);
    }

    const int32_t lx = gp->axis_x - joystick_origin_lx;
    const int32_t ly = gp->axis_y - joystick_origin_ly;
    const int32_t rx = gp->axis_rx - joystick_origin_rx;
    const int32_t ry = gp->axis_ry - joystick_origin_ry;

    apply_gamepad_to_servos(lx, ly, rx, ry, gp->brake, gp->throttle, gp->buttons);

    const int64_t now_us = esp_timer_get_time();
    if ((now_us - last_log_us) < 100000) {
        skipped_frames++;
        return;
    }
    last_log_us = now_us;

    printf("[PAD] lx=%4ld ly=%4ld | rx=%4ld ry=%4ld | L2=%4ld R2=%4ld | btns=0x%04x misc=0x%02x | skip=%lu\n",
           (long)lx, (long)ly,
           (long)rx, (long)ry,
           (long)gp->brake, (long)gp->throttle,
           gp->buttons, gp->misc_buttons,
           (unsigned long)skipped_frames);
    skipped_frames = 0;
}

static const uni_property_t* on_get_property(uni_property_idx_t idx) {
    (void)idx;
    return NULL;
}

static void on_oob_event(uni_platform_oob_event_t event, void* data) {
    (void)data;
    if (event == UNI_PLATFORM_OOB_BLUETOOTH_ENABLED) {
        printf("[BT] Scan actif\n");
    }
}

static struct uni_platform custom_platform = {
    .name = "SpiderBot BT Test",
    .init = on_init,
    .on_init_complete = on_init_complete,
    .on_device_discovered = on_device_discovered,
    .on_device_connected = on_device_connected,
    .on_device_disconnected = on_device_disconnected,
    .on_device_ready = on_device_ready,
    .on_gamepad_data = on_gamepad_data,
    .on_controller_data = NULL,
    .get_property = on_get_property,
    .on_oob_event = on_oob_event,
    .device_dump = NULL,
    .register_console_cmds = NULL,
};

void app_main(void) {
    btstack_init();
    uni_platform_set_custom(&custom_platform);
    uni_init(0, NULL);
    btstack_run_loop_execute();
}
