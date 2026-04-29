// Test minimal Bluepad32: connecte la DualSense et log les entrees.
// Compile uniquement avec le profil esp32dev-bt-test (SPIDERBOT_BT_TEST=1).
// Aucun servo, aucune FSM, aucune dependance Arduino.

#include <stdio.h>

#include "controller/uni_gamepad.h"
#include "platform/uni_platform.h"
#include "uni.h"
#include "uni_hid_device.h"

static void on_init(int argc, const char** argv) {
    (void)argc;
    (void)argv;
    printf("\n[BT] Spider-Bot Bluepad32 test demarre\n");
}

static void on_init_complete(void) {
    printf("[BT] Bluetooth pret\n");
    printf("[BT] --> Maintenez PS + Create sur la DualSense pour appairer\n");
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
    printf("[BT] Manette deconnectee\n");
}

static uni_error_t on_device_ready(uni_hid_device_t* d) {
    (void)d;
    printf("[BT] DualSense prete! Les entrees vont apparaitre ci-dessous.\n");
    return UNI_ERROR_SUCCESS;
}

static void on_gamepad_data(uni_hid_device_t* d, uni_gamepad_t* gp) {
    (void)d;
    printf("[PAD] lx=%4d ly=%4d | rx=%4d ry=%4d | L2=%4d R2=%4d | btns=0x%04x misc=0x%02x\n",
           gp->axis_x, gp->axis_y,
           gp->axis_rx, gp->axis_ry,
           gp->brake, gp->throttle,
           gp->buttons, gp->misc_buttons);
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

struct uni_platform* uni_platform_custom_create(void) {
    return &custom_platform;
}

void app_main(void) {
    uni_init(0, NULL);
}
