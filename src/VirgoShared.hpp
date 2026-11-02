#pragma once
#include <Geode/Geode.hpp>
#include <algorithm>
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

// Persistent user preferences. Autoplay fields are stored now so future
// Brain/Autoplay controls can use the same save system when those features land.
inline bool settingsLoaded = false;
inline bool bufferEnabled = true;
inline int bufferPresetIndex = 0;
inline int selectedStage = 0;
inline bool quietMode = false;
inline bool animationsEnabled = true;
inline bool brainLearningEnabled = true;
inline bool brainLessonsEnabled = true;
inline bool brainAutoTuneEnabled = false;
inline int brainAutoTuneMode = 1; // 0 = Light, 1 = Medium, 2 = Aggressive
inline bool autoPlayEnabled = false;
inline int autoPlayMode = 0;     // 0 = Normal, 1 = Practice
inline int autoPlayPreset = -1;  // -1 = no preset selected

// Persistent user preferences. Autoplay fields are stored now so future
// Brain/Autoplay controls can use the same save system when those features land.
inline bool settingsLoaded = false;
inline bool bufferEnabled = true;
inline int bufferPresetIndex = 0;
inline int selectedStage = 0;
inline bool quietMode = false;
inline bool animationsEnabled = true;
inline bool brainLearningEnabled = true;
inline bool brainLessonsEnabled = true;
inline bool brainAutoTuneEnabled = false;
inline int brainAutoTuneMode = 1; // 0 = Light, 1 = Medium, 2 = Aggressive
inline bool autoPlayEnabled = false;
inline int autoPlayMode = 0;     // 0 = Normal, 1 = Practice
inline int autoPlayPreset = -1;  // -1 = no preset selected

// Brain foundation: real telemetry, persisted by Geode between game sessions.
inline std::array<std::uint64_t, ModeCount> predictionsByMode = {};
inline std::array<std::uint64_t, ModeCount> safeDelaysByMode = {};
inline std::array<std::uint64_t, ModeCount> deathsByMode = {};
inline bool brainDataLoaded = false;

inline constexpr std::array<const char*, ModeCount> modeSaveKeys = {
    "cube", "wave", "ship", "ball", "ufo", "robot", "spider", "swing"
};

inline void loadSettings() {
    if (settingsLoaded) return;
    auto* mod = geode::Mod::get();
    if (!mod) return;

    for (int i = 0; i < ModeCount; ++i) {
        const auto index = static_cast<std::size_t>(i);
        const std::string mode = modeSaveKeys[index];
        modeEnabled[index] = mod->getSavedValue<bool>(std::string("settings.mode.enabled.") + mode, modeEnabled[index]);
    }

    modeFrames[Cube] = std::clamp(mod->getSavedValue<int>("settings.frames.cube", modeFrames[Cube]), 0, 60);
    modeFrames[Wave] = std::clamp(mod->getSavedValue<int>("settings.frames.wave", modeFrames[Wave]), 0, 60);
    modeFrames[Ship] = std::clamp(mod->getSavedValue<int>("settings.frames.ship", modeFrames[Ship]), 0, 60);

    bufferEnabled = mod->getSavedValue<bool>("settings.buffer.enabled", bufferEnabled);
    bufferPresetIndex = std::clamp(mod->getSavedValue<int>("settings.buffer.preset", bufferPresetIndex), 0, 9);
    selectedStage = std::clamp(mod->getSavedValue<int>("settings.stage.selected", selectedStage), 0, 5);
    quietMode = mod->getSavedValue<bool>("settings.ui.quiet", quietMode);
    animationsEnabled = mod->getSavedValue<bool>("settings.ui.animations", animationsEnabled);

    brainLearningEnabled = mod->getSavedValue<bool>("settings.brain.learning", brainLearningEnabled);
    brainLessonsEnabled = mod->getSavedValue<bool>("settings.brain.lessons", brainLessonsEnabled);
    brainAutoTuneEnabled = mod->getSavedValue<bool>("settings.brain.autotune.enabled", brainAutoTuneEnabled);
    brainAutoTuneMode = std::clamp(mod->getSavedValue<int>("settings.brain.autotune.mode", brainAutoTuneMode), 0, 2);

    autoPlayEnabled = mod->getSavedValue<bool>("settings.autoplay.enabled", autoPlayEnabled);
    autoPlayMode = std::clamp(mod->getSavedValue<int>("settings.autoplay.mode", autoPlayMode), 0, 1);
    autoPlayPreset = std::clamp(mod->getSavedValue<int>("settings.autoplay.preset", autoPlayPreset), -1, 3);

    settingsLoaded = true;
}

inline void saveSettings() {
    loadSettings();
    auto* mod = geode::Mod::get();
    if (!mod || !settingsLoaded) return;

    for (int i = 0; i < ModeCount; ++i) {
        const auto index = static_cast<std::size_t>(i);
        mod->setSavedValue<bool>(std::string("settings.mode.enabled.") + modeSaveKeys[index], modeEnabled[index]);
    }

    mod->setSavedValue<int>("settings.frames.cube", modeFrames[Cube]);
    mod->setSavedValue<int>("settings.frames.wave", modeFrames[Wave]);
    mod->setSavedValue<int>("settings.frames.ship", modeFrames[Ship]);
    mod->setSavedValue<bool>("settings.buffer.enabled", bufferEnabled);
    mod->setSavedValue<int>("settings.buffer.preset", bufferPresetIndex);
    mod->setSavedValue<int>("settings.stage.selected", selectedStage);
    mod->setSavedValue<bool>("settings.ui.quiet", quietMode);
    mod->setSavedValue<bool>("settings.ui.animations", animationsEnabled);

    mod->setSavedValue<bool>("settings.brain.learning", brainLearningEnabled);
    mod->setSavedValue<bool>("settings.brain.lessons", brainLessonsEnabled);
    mod->setSavedValue<bool>("settings.brain.autotune.enabled", brainAutoTuneEnabled);
    mod->setSavedValue<int>("settings.brain.autotune.mode", brainAutoTuneMode);

    mod->setSavedValue<bool>("settings.autoplay.enabled", autoPlayEnabled);
    mod->setSavedValue<int>("settings.autoplay.mode", autoPlayMode);
    mod->setSavedValue<int>("settings.autoplay.preset", autoPlayPreset);
}

inline void loadSettings() {
    if (settingsLoaded) return;
    auto* mod = geode::Mod::get();
    if (!mod) return;

    for (int i = 0; i < ModeCount; ++i) {
        const auto index = static_cast<std::size_t>(i);
        const std::string mode = modeSaveKeys[index];
        modeEnabled[index] = mod->getSavedValue<bool>(std::string("settings.mode.enabled.") + mode, modeEnabled[index]);
    }

    modeFrames[Cube] = std::clamp(mod->getSavedValue<int>("settings.frames.cube", modeFrames[Cube]), 0, 60);
    modeFrames[Wave] = std::clamp(mod->getSavedValue<int>("settings.frames.wave", modeFrames[Wave]), 0, 60);
    modeFrames[Ship] = std::clamp(mod->getSavedValue<int>("settings.frames.ship", modeFrames[Ship]), 0, 60);

    bufferEnabled = mod->getSavedValue<bool>("settings.buffer.enabled", bufferEnabled);
    bufferPresetIndex = std::clamp(mod->getSavedValue<int>("settings.buffer.preset", bufferPresetIndex), 0, 9);
    selectedStage = std::clamp(mod->getSavedValue<int>("settings.stage.selected", selectedStage), 0, 5);
    quietMode = mod->getSavedValue<bool>("settings.ui.quiet", quietMode);
    animationsEnabled = mod->getSavedValue<bool>("settings.ui.animations", animationsEnabled);

    brainLearningEnabled = mod->getSavedValue<bool>("settings.brain.learning", brainLearningEnabled);
    brainLessonsEnabled = mod->getSavedValue<bool>("settings.brain.lessons", brainLessonsEnabled);
    brainAutoTuneEnabled = mod->getSavedValue<bool>("settings.brain.autotune.enabled", brainAutoTuneEnabled);
    brainAutoTuneMode = std::clamp(mod->getSavedValue<int>("settings.brain.autotune.mode", brainAutoTuneMode), 0, 2);

    autoPlayEnabled = mod->getSavedValue<bool>("settings.autoplay.enabled", autoPlayEnabled);
    autoPlayMode = std::clamp(mod->getSavedValue<int>("settings.autoplay.mode", autoPlayMode), 0, 1);
    autoPlayPreset = std::clamp(mod->getSavedValue<int>("settings.autoplay.preset", autoPlayPreset), -1, 3);

    settingsLoaded = true;
}

inline void saveSettings() {
    loadSettings();
    auto* mod = geode::Mod::get();
    if (!mod || !settingsLoaded) return;

    for (int i = 0; i < ModeCount; ++i) {
        const auto index = static_cast<std::size_t>(i);
        mod->setSavedValue<bool>(std::string("settings.mode.enabled.") + modeSaveKeys[index], modeEnabled[index]);
    }

    mod->setSavedValue<int>("settings.frames.cube", modeFrames[Cube]);
    mod->setSavedValue<int>("settings.frames.wave", modeFrames[Wave]);
    mod->setSavedValue<int>("settings.frames.ship", modeFrames[Ship]);
    mod->setSavedValue<bool>("settings.buffer.enabled", bufferEnabled);
    mod->setSavedValue<int>("settings.buffer.preset", bufferPresetIndex);
    mod->setSavedValue<int>("settings.stage.selected", selectedStage);
    mod->setSavedValue<bool>("settings.ui.quiet", quietMode);
    mod->setSavedValue<bool>("settings.ui.animations", animationsEnabled);

    mod->setSavedValue<bool>("settings.brain.learning", brainLearningEnabled);
    mod->setSavedValue<bool>("settings.brain.lessons", brainLessonsEnabled);
    mod->setSavedValue<bool>("settings.brain.autotune.enabled", brainAutoTuneEnabled);
    mod->setSavedValue<int>("settings.brain.autotune.mode", brainAutoTuneMode);

    mod->setSavedValue<bool>("settings.autoplay.enabled", autoPlayEnabled);
    mod->setSavedValue<int>("settings.autoplay.mode", autoPlayMode);
    mod->setSavedValue<int>("settings.autoplay.preset", autoPlayPreset);
}

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
