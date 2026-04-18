#pragma once

#include <map>
#include <optional>
#include <string>

namespace spiderbot {

// Etat normalise de la manette.
// - axes: valeurs continuees (souvent dans [-1, 1])
// - buttons: etats booleens appuye/non appuye
struct GamepadData {
    std::map<std::string, bool> buttons;
    std::map<std::string, double> axes;
};

// Adaptateur de la source manette.
// Le but est de decoupler la logique robot d'une implementation SDK specifique.
class BluePadManager {
public:
    BluePadManager();

    // Lit une nouvelle trame manette.
    // Retourne std::nullopt si aucune donnee n'est disponible.
    std::optional<GamepadData> update();

    // Indique l'etat de connexion estime de la manette.
    bool isConnected() const;

    // Retourne la derniere trame connue, si disponible.
    std::optional<GamepadData> getGamepadData() const;

    // Interface d'integration: publier une trame recue depuis la couche SDK.
    // Exemple: callback Bluepad32 -> publishFrame(...).
    void publishFrame(const GamepadData& frame);

    // Interface d'integration: signaler explicitement une deconnexion manette.
    void notifyDisconnected();

private:
    // Etat de lien courant.
    bool connected_;

    // Cache de la derniere trame utile pour debug/inspection.
    std::optional<GamepadData> gamepadData_;

    // Indique qu'une nouvelle trame a ete publiee depuis le dernier update().
    bool hasFreshFrame_;
};

}  // namespace spiderbot
