#pragma once

#include <map>
#include <string>
#include <vector>

namespace spiderbot {

struct CommandMessage {
    std::string type;
    std::map<std::string, std::string> data;
};

class Command {
public:
    static std::string serialize(const std::string& commandType,
                                 const std::map<std::string, std::string>& data);
    static CommandMessage deserialize(const std::string& jsonLike);
};

struct CommandTypes {
    static constexpr const char* GAMEPAD = "gamepad";
    static constexpr const char* SERVO_DIRECT = "servo_direct";
    static constexpr const char* SERVO_BATCH = "servo_batch";
    static constexpr const char* STATUS_REQUEST = "status_request";
    static constexpr const char* STATUS_RESPONSE = "status_response";
    static constexpr const char* VIDEO_FRAME = "video_frame";
    static constexpr const char* AUDIO_CHUNK = "audio_chunk";
    static constexpr const char* ERROR = "error";
};

std::string createGamepadCommand(const std::string& buttonsJson,
                                 const std::string& axesJson);

std::string createServoDirectCommand(int servoId, int angle);

std::string createServoBatchCommand(const std::vector<int>& angles);

std::string createStatusRequest();

std::string createStatusResponse(const std::string& esp32Status,
                                 const std::string& raspberryStatus);

}  // namespace spiderbot
