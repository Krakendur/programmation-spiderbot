#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace spiderbot::protocol {

inline constexpr std::uint8_t kProtocolVersion = 1;
inline constexpr std::size_t kPayloadSize = 16;

enum class PacketType : std::uint8_t {
    None = 0,
    Heartbeat = 1,
    MotionStatus = 2,
    AvStatus = 3,
    Command = 4,
};

struct Packet {
    std::uint8_t version{kProtocolVersion};
    PacketType type{PacketType::None};
    std::array<std::uint8_t, kPayloadSize> payload{};
};

bool isValid(const Packet& packet);

}  // namespace spiderbot::protocol
