#include "ble_controller.hpp"
#include "display_manager.hpp"
#include "menu_input.hpp"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstring>

namespace spiderbot {

static const char* TAG = "BLE";

// ── Singleton ─────────────────────────────────────────────────────────────────
BleController* BleController::sInstance_ = nullptr;

// ── File-scope GATT state (used by C callbacks and service definition) ─────────
static uint16_t            s_valHandle = 0;
static SpiderControlPacket s_lastPkt   = {};

// ── Service UUID : AA000001-0000-1000-8000-00805F9B34FB (little-endian bytes) ──
static const ble_uuid128_t kSvcUuid = BLE_UUID128_INIT(
    0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x01, 0x00, 0x00, 0xAA);

// ── Characteristic UUID : AA000002-0000-1000-8000-00805F9B34FB ─────────────────
static const ble_uuid128_t kChrUuid = BLE_UUID128_INIT(
    0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x02, 0x00, 0x00, 0xAA);

// ── GATT characteristic access callback (READ) ─────────────────────────────────
static int chrCb(uint16_t, uint16_t, struct ble_gatt_access_ctxt* ctxt, void*) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        int rc = os_mbuf_append(ctxt->om, &s_lastPkt, sizeof(s_lastPkt));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

// ── GAP callback — routes to BleController instance via 'arg' ─────────────────
static int gapCb(struct ble_gap_event* event, void* arg) {
    return static_cast<BleController*>(arg)->onGapEvent(event);
}

// ── GATT service definition (static lifetime required by NimBLE) ───────────────
static const struct ble_gatt_chr_def kChrDefs[] = {
    {
        .uuid       = &kChrUuid.u,
        .access_cb  = chrCb,
        .flags      = BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_READ,
        .val_handle = &s_valHandle,
    },
    { 0 }
};

static const struct ble_gatt_svc_def kSvcDefs[] = {
    {
        .type            = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid            = &kSvcUuid.u,
        .characteristics = kChrDefs,
    },
    { 0 }
};

// ── NimBLE host task — runs on Core 0 ─────────────────────────────────────────
void BleController::hostTask(void*) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}

// ── Called by NimBLE when the host stack is synchronised and ready ─────────────
void BleController::onSync() {
    if (!sInstance_) return;
    ble_hs_id_infer_auto(0, &sInstance_->addrType_);
    sInstance_->startAdvertising();
}

void BleController::onReset(int reason) {
    ESP_LOGE(TAG, "NimBLE host reset, reason=%d", reason);
}

// ── GAP event handler (called from Core 0 via gapCb) ─────────────────────────
int BleController::onGapEvent(struct ble_gap_event* event) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                connHandle_ = event->connect.conn_handle;  // set before connected_ flag
                connected_  = true;
                ESP_LOGI(TAG, "Connected, handle=%d", connHandle_);
            } else {
                connected_ = false;
                startAdvertising();
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Disconnected, reason=%d", event->disconnect.reason);
            connected_  = false;
            subscribed_ = false;
            connHandle_ = 0xFFFF;
            startAdvertising();
            break;

        case BLE_GAP_EVENT_SUBSCRIBE:
            if (event->subscribe.attr_handle == s_valHandle) {
                subscribed_ = (event->subscribe.cur_notify != 0);
                ESP_LOGI(TAG, "Notify subscription: %s", subscribed_ ? "on" : "off");
            }
            break;

        default:
            break;
    }
    return 0;
}

// ── Start BLE advertising ─────────────────────────────────────────────────────
void BleController::startAdvertising() {
    struct ble_hs_adv_fields fields = {};
    const char* name = "SpiderController";
    fields.name             = (const uint8_t*)name;
    fields.name_len         = static_cast<uint8_t>(strlen(name));
    fields.name_is_complete = 1;
    fields.flags            = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) { ESP_LOGE(TAG, "adv_set_fields: %d", rc); return; }

    struct ble_gap_adv_params params = {};
    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(addrType_, nullptr, BLE_HS_FOREVER, &params, gapCb, this);
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "adv_start: %d", rc);
        return;
    }
    ESP_LOGI(TAG, "Advertising as 'SpiderController'");
}

// ── Send notification to connected central ────────────────────────────────────
void BleController::sendPacket(const SpiderControlPacket& pkt) {
    s_lastPkt = pkt;
    if (!connected_ || !subscribed_) return;

    struct os_mbuf* om = ble_hs_mbuf_from_flat(&pkt, sizeof(pkt));
    if (!om) { ESP_LOGW(TAG, "mbuf alloc failed"); return; }

    int rc = ble_gatts_notify_custom(connHandle_, s_valHandle, om);
    if (rc != 0) ESP_LOGD(TAG, "notify_custom: %d", rc);
}

// ── Initialize NimBLE stack ───────────────────────────────────────────────────
bool BleController::initialize(DisplayManager& display) {
    display_   = &display;
    sInstance_ = this;

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS corrupt, erasing");
        nvs_flash_erase();
        nvs_flash_init();
    }

    if (nimble_port_init() != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed");
        return false;
    }

    ble_hs_cfg.reset_cb = onReset;
    ble_hs_cfg.sync_cb  = onSync;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_gatts_count_cfg(kSvcDefs);
    ble_gatts_add_svcs(kSvcDefs);
    ble_svc_gap_device_name_set("SpiderController");

    nimble_port_freertos_init(hostTask);

    ESP_LOGI(TAG, "BLE init OK");
    return true;
}

// ── Main loop — 50 Hz, blocking ──────────────────────────────────────────────
void BleController::run(MenuInput& input) {
    display_->showBleStatus("En attente...");

    bool     prevConnected = false;
    uint32_t lastLogoMs    = static_cast<uint32_t>(esp_timer_get_time() / 1000LL);

    while (true) {
        uint32_t nowMs = static_cast<uint32_t>(esp_timer_get_time() / 1000LL);

        // Refresh full screen only when connection state changes
        if (connected_ != prevConnected) {
            prevConnected = connected_;
            display_->showBleStatus(connected_ ? "Connecte" : "En attente...");
            lastLogoMs = nowMs;
        }

        // Animate logo at ~20 Hz (50 ms)
        if (nowMs - lastLogoMs >= 50) {
            lastLogoMs = nowMs;
            display_->tickBleLogo();
        }

        // Read inputs and build packet
        SpiderControlPacket pkt{};
        pkt.lx   = static_cast<int16_t>(input.readAxisX() * 1000.0f);
        pkt.ly   = static_cast<int16_t>(input.readAxisY() * 1000.0f);
        pkt.rx   = 0;
        pkt.ry   = 0;
        pkt.btnA = input.readBtnSelect() ? 1 : 0;
        pkt.btnB = input.readBtnBack()   ? 1 : 0;

        sendPacket(pkt);

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void BleController::shutdown() {
    nimble_port_stop();
    connected_  = false;
    subscribed_ = false;
}

}  // namespace spiderbot
