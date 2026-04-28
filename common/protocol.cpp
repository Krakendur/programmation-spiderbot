#include "protocol.hpp"

namespace spiderbot::protocol {

bool isValid(const Packet& packet) {
    return packet.version == kProtocolVersion && packet.type != PacketType::None;
}

}  // namespace spiderbot::protocol
