// Test minimal Bluepad32: connecte la DualSense et log les entrees.
// Compile uniquement avec le profil esp32dev-bt-test (SPIDERBOT_BT_TEST=1).
// Aucun servo, aucune FSM, aucune dependance Arduino.

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include <btstack_port_esp32.h>
#include <btstack_run_loop.h>

#include "bt/uni_bt.h"
#include "controller/uni_gamepad.h"
#include "esp_timer.h"
#include "platform/uni_platform.h"
#include "uni.h"
#include "uni_hid_device.h"

static bool joystick_origin_captured = false;
static int32_t joystick_origin_lx = 0;
static int32_t joystick_origin_ly = 0;
static int32_t joystick_origin_rx = 0;
static int32_t joystick_origin_ry = 0;

static void on_init(int argc, const char** argv) {
    (void)argc;
    (void)argv;
    printf("\n[BT] Spider-Bot Bluepad32 test demarre\n");
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

    const int64_t now_us = esp_timer_get_time();
    if ((now_us - last_log_us) < 100000) {
        skipped_frames++;
        return;
    }
    last_log_us = now_us;

    const int32_t lx = gp->axis_x - joystick_origin_lx;
    const int32_t ly = gp->axis_y - joystick_origin_ly;
    const int32_t rx = gp->axis_rx - joystick_origin_rx;
    const int32_t ry = gp->axis_ry - joystick_origin_ry;

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
