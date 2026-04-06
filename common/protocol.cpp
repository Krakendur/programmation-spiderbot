#include "protocol.hpp"

#include <sstream>

namespace spiderbot {

namespace {

std::string quote(const std::string& value) {
    std::string out = "\"";
    for (char c : value) {
        if (c == '"') {
            out += "\\\"";
        } else {
            out += c;
        }
    }
    out += "\"";
    return out;
}

std::string joinAngles(const std::vector<int>& angles) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < angles.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }
        oss << angles[i];
    }
    oss << "]";
    return oss.str();
}

}  // namespace

std::string Command::serialize(const std::string& commandType,
                               const std::map<std::string, std::string>& data) {
    std::ostringstream oss;
    oss << "{\"type\":" << quote(commandType) << ",\"data\":{";

    bool first = true;
    for (const auto& kv : data) {
        if (!first) {
            oss << ",";
        }
        first = false;
        oss << quote(kv.first) << ":" << kv.second;
    }

    oss << "}}";
    return oss.str();
}

CommandMessage Command::deserialize(const std::string& jsonLike) {
    // Minimal parser for debugging. Production code should use a JSON library.
    CommandMessage msg;
    const std::string key = "\"type\":\"";
    const auto start = jsonLike.find(key);
    if (start == std::string::npos) {
        return msg;
    }

    const auto valueStart = start + key.size();
    const auto valueEnd = jsonLike.find('"', valueStart);
    if (valueEnd == std::string::npos) {
        return msg;
    }

    msg.type = jsonLike.substr(valueStart, valueEnd - valueStart);
    return msg;
}

std::string createGamepadCommand(const std::string& buttonsJson,
                                 const std::string& axesJson) {
    return Command::serialize(
        CommandTypes::GAMEPAD,
        {
            {"buttons", buttonsJson},
            {"axes", axesJson},
        });
}

std::string createServoDirectCommand(int servoId, int angle) {
    return Command::serialize(
        CommandTypes::SERVO_DIRECT,
        {
            {"servo_id", std::to_string(servoId)},
            {"angle", std::to_string(angle)},
        });
}

std::string createServoBatchCommand(const std::vector<int>& angles) {
    return Command::serialize(
        CommandTypes::SERVO_BATCH,
        {
            {"angles", joinAngles(angles)},
            {"count", std::to_string(angles.size())},
        });
}

std::string createStatusRequest() {
    return Command::serialize(CommandTypes::STATUS_REQUEST, {});
}

std::string createStatusResponse(const std::string& esp32Status,
                                 const std::string& raspberryStatus) {
    return Command::serialize(
        CommandTypes::STATUS_RESPONSE,
        {
            {"esp32", quote(esp32Status)},
            {"raspberry", quote(raspberryStatus)},
        });
}

}  // namespace spiderbot
