#pragma once
#include <Geode/Geode.hpp>
#include <array>
#include <string>
#include <cstdint>

// Shared runtime state for the VIRGO UI and predictive input engine.
namespace virgo {
enum GameMode : int {
    Cube = 0,
    Wave = 1,
    Ship = 2,
    Ball = 3,
    UFO = 4,
    Robot = 5,
    Spider = 6,
    Swing = 7,
    ModeCount = 8
};

inline std::array<bool, ModeCount> modeEnabled = {
    true, true, false, true, true, true, true, true
};

inline std::array<int, 3> modeFrames = {2, 2, 2};

// Brain foundation: real telemetry, persisted by Geode between game sessions.
inline std::array<std::uint64_t, ModeCount> predictionsByMode = {};
inline std::array<std::uint64_t, ModeCount> safeDelaysByMode = {};
inline std::array<std::uint64_t, ModeCount> deathsByMode = {};
inline bool brainDataLoaded = false;

inline constexpr std::array<const char*, ModeCount> modeSaveKeys = {
    "cube", "wave", "ship", "ball", "ufo", "robot", "spider", "swing"
};

inline void loadBrainData() {
    if (brainDataLoaded) return;
    auto* mod = geode::Mod::get();
    if (!mod) return;

    for (int i = 0; i < ModeCount; ++i) {
        const std::string mode = modeSaveKeys[static_cast<std::size_t>(i)];
        predictionsByMode[static_cast<std::size_t>(i)] =
            mod->getSavedValue<std::uint64_t>(std::string("brain.predictions.") + mode, std::uint64_t{0});
        safeDelaysByMode[static_cast<std::size_t>(i)] =
            mod->getSavedValue<std::uint64_t>(std::string("brain.safe-delays.") + mode, std::uint64_t{0});
        deathsByMode[static_cast<std::size_t>(i)] =
            mod->getSavedValue<std::uint64_t>(std::string("brain.deaths.") + mode, std::uint64_t{0});
    }
    brainDataLoaded = true;
}

inline void recordPrediction(int mode) {
    loadBrainData();
    if (!brainDataLoaded || mode < 0 || mode >= ModeCount) return;
    const auto index = static_cast<std::size_t>(mode);
    ++predictionsByMode[index];
    geode::Mod::get()->setSavedValue<std::uint64_t>(
        std::string("brain.predictions.") + modeSaveKeys[index], predictionsByMode[index]
    );
}

inline void recordSafeDelay(int mode) {
    loadBrainData();
    if (!brainDataLoaded || mode < 0 || mode >= ModeCount) return;
    const auto index = static_cast<std::size_t>(mode);
    ++safeDelaysByMode[index];
    geode::Mod::get()->setSavedValue<std::uint64_t>(
        std::string("brain.safe-delays.") + modeSaveKeys[index], safeDelaysByMode[index]
    );
}

inline void recordDeath(int mode) {
    loadBrainData();
    if (!brainDataLoaded || mode < 0 || mode >= ModeCount) return;
    const auto index = static_cast<std::size_t>(mode);
    ++deathsByMode[index];
    geode::Mod::get()->setSavedValue<std::uint64_t>(
        std::string("brain.deaths.") + modeSaveKeys[index], deathsByMode[index]
    );
}

inline std::uint64_t totalPredictions() {
    std::uint64_t total = 0;
    for (auto value : predictionsByMode) total += value;
    return total;
}

inline std::uint64_t totalSafeDelays() {
    std::uint64_t total = 0;
    for (auto value : safeDelaysByMode) total += value;
    return total;
}

inline std::uint64_t totalDeaths() {
    std::uint64_t total = 0;
    for (auto value : deathsByMode) total += value;
    return total;
}
}
