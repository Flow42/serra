#pragma once
#include <string>

class DSPEngine;

class PresetManager {
public:
    // Serializes the engine's current state to a JSON string.
    static std::string savePreset(const DSPEngine& engine);
    
    // Deserializes the JSON string and applies the state to the engine.
    static bool loadPreset(DSPEngine& engine, const std::string& presetJson);
};
