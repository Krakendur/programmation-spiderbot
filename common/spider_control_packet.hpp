#pragma once
#include <cstdint>

// Paquet de 10 octets envoyé par le contrôleur (ESP32-C6) au robot (ESP32)
// via BLE GATT (characteristic AA000002-...).
// Format compatible avec GamepadData de retour_platformio :
//   axes["lx"] = lx / 1000.0,  axes["ly"] = ly / 1000.0
//   buttons["A"] = btnA,        buttons["B"] = btnB
#pragma pack(push, 1)
struct SpiderControlPacket {
    int16_t lx;      // -1000 … +1000  axe joystick X (gauche = négatif)
    int16_t ly;      // -1000 … +1000  axe joystick Y (haut = négatif)
    int16_t rx;      // -1000 … +1000  réservé (= 0)
    int16_t ry;      // -1000 … +1000  réservé (= 0)
    uint8_t btnA;    // BTN1 (GPIO2) — validation / avancer
    uint8_t btnB;    // BTN2 (GPIO3) — retour / stop
};
#pragma pack(pop)
