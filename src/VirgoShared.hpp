#pragma once
#include <array>
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

inline std::array<std::uint64_t, ModeCount> predictionsByMode = {};
inline std::array<std::uint64_t, ModeCount> safeDelaysByMode = {};
inline std::array<std::uint64_t, ModeCount> deathsByMode = {};
}
