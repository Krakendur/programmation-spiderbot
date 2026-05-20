#pragma once

#include <cstdint>
#include "spider_control_packet.hpp"

// Forward declarations — full NimBLE headers only in .cpp
struct ble_gap_event;

namespace spiderbot {

class DisplayManager;
class MenuInput;

class BleController {
public:
    bool initialize(DisplayManager& display);
    void run(MenuInput& input);   // blocking 50 Hz loop
    void shutdown();
    bool isConnected() const { return connected_; }

    // Called from the file-scope GAP callback via the 'arg' pointer
    int onGapEvent(struct ble_gap_event* event);

private:
    static void hostTask(void* param);  // NimBLE host FreeRTOS task (Core 0)
    static void onSync();               // NimBLE ready → start advertising
    static void onReset(int reason);

    void startAdvertising();
    void sendPacket(const SpiderControlPacket& pkt);

    DisplayManager* display_{nullptr};
    uint8_t         addrType_{0};
    uint16_t        connHandle_{0xFFFF};
    bool            connected_{false};
    bool            subscribed_{false};

    static BleController* sInstance_;
};

}  // namespace spiderbot
