#pragma once

#include <chrono>

namespace spiderbot {

// Horloge monotone en millisecondes.
// Utilisee pour la boucle temps reel et les phases de marche.
inline long long nowMs() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

}  // namespace spiderbot