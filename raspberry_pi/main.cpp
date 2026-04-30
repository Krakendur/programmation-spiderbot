#include "av_fsm.hpp"

#include <csignal>
#include <iostream>
#include <thread>
#include <chrono>

static std::atomic<bool> g_running{true};

static void on_signal(int) { g_running.store(false); }

int main() {
    std::signal(SIGINT,  on_signal);
    std::signal(SIGTERM, on_signal);

    std::cout << "Spider-Bot Raspberry Pi - Demarrage audiovisuel\n";

    spiderbot::AvFsm av;

    if (!av.init()) {
        std::cerr << "Echec initialisation AV\n";
        return 1;
    }

    // Démarre en mode AV complet (caméra + micro)
    if (!av.startAV()) {
        std::cerr << "Echec demarrage AV_MODE\n";
        return 1;
    }

    std::cout << "Systeme actif - Ctrl+C pour arreter\n";

    while (g_running.load()) {
        // La boucle principale reste légère : le travail est dans les threads
        // des sous-gestionnaires (captureLoop, dispatchLoop).
        // Ici on pourrait surveiller l'état et déclencher un reset si ERROR_AV.
        if (av.state() == spiderbot::AvState::ERROR_AV) {
            std::cerr << "[Main] Erreur AV détectée - tentative de reset\n";
            av.reset();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\nArrêt demandé\n";
    av.safeStop();
    return 0;
}
