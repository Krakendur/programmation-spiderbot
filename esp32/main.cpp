#include "robot_controller.hpp"

#include <iostream>
#include <stdexcept>
#include <vector>

namespace {

std::vector<int> getServoPins() {
    // Mapping logique servo_id -> GPIO. A adapter au cablage reel.
    return {2, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32};
}

}  // namespace

int main() {
    const auto servoPins = getServoPins();
    std::cout << "Spider-Bot ESP32 C++ - Initialization" << std::endl;

    try {
        spiderbot::RobotController controller(servoPins);
        return controller.run();
    } catch (const std::invalid_argument& e) {
        std::cerr << "[ERROR] " << e.what() << std::endl;
        return 1;
    }
}
