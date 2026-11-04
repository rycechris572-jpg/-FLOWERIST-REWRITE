#pragma once

#include "VirgoShared.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <unordered_set>
#include <limits>

namespace virgo {

// Runtime identity for the level currently being played.
// Level ID zero is treated as untracked instead of merging unrelated levels.
inline int brainActiveLevelId = 0;
inline int brainActiveGameMode = Cube;

struct BrainLevelObject {
    int objectId = 0;
    int objectType = 0;
    float x = 0.f;
    float y = 0.f;
    int qx = 0;
    int qy = 0;
};

inline std::vector<BrainLevelObject> brainLevelObjects;
inline std::unordered_set<std::string> brainLevelObjectKeys;

inline std::string brainLevelPrefix(int levelId) {
    return std::string("brain.level.") + std::to_string(levelId) + ".";
}

inline std::uint64_t brainReadCount(const std::string& key) {
    auto* mod = geode::Mod::get();
    return mod ? mod->getSavedValue<std::uint64_t>(key, std::uint64_t{0}) : 0;
}

inline void brainWriteCount(const std::string& key, std::uint64_t value) {
    if (auto* mod = geode::Mod::get()) {
        mod->setSavedValue<std::uint64_t>(key, value);
    }
}

inline void brainIncrement(const std::string& key) {
    brainWriteCount(key, brainReadCount(key) + 1);
}

inline std::string brainModeSuffix(int mode) {
    if (mode < 0 || mode >= ModeCount) mode = Cube;
    return std::string("mode.") + modeSaveKeys[static_cast<std::size_t>(mode)] + ".";
}

inline void brainBeginLevel(int levelId, int mode) {
    brainActiveLevelId = levelId;
    brainActiveGameMode = (mode >= 0 && mode < ModeCount) ? mode : Cube;
    brainLevelObjects.clear();
    brainLevelObjectKeys.clear();

    if (!brainLearningEnabled || levelId == 0) return;
    auto* mod = geode::Mod::get();
    if (!mod) return;

    const auto prefix = brainLevelPrefix(levelId);
    const bool alreadyKnown = mod->getSavedValue<bool>(prefix + "known", false);
    if (!alreadyKnown) {
        mod->setSavedValue<bool>(prefix + "known", true);
        brainIncrement("brain.levels.known");
    }

    brainIncrement(prefix + "sessions");
    brainIncrement(prefix + brainModeSuffix(brainActiveGameMode) + "sessions");
}

inline void brainRecordProgress(float x) {
    if (!brainLearningEnabled || brainActiveLevelId == 0) return;
    auto* mod = geode::Mod::get();
    if (!mod) return;

    // Save progress in 25-unit steps to avoid writing saved data every frame.
    const int progressBucket = std::max(0, static_cast<int>(x / 25.f)) * 25;
    const auto key = brainLevelPrefix(brainActiveLevelId) + "best-x";
    const int previous = mod->getSavedValue<int>(key, 0);
    if (progressBucket > previous) {
        mod->setSavedValue<int>(key, progressBucket);
    }
}

inline void brainRecordLevelDeath(int mode, float x) {
    if (!brainLearningEnabled || brainActiveLevelId == 0) return;
    brainRecordProgress(x);

    const auto prefix = brainLevelPrefix(brainActiveLevelId);
    brainIncrement(prefix + "deaths");
    brainIncrement(prefix + brainModeSuffix(mode) + "deaths");
}

inline std::string brainObjectKey(const char* kind, int objectId, float x, float y) {
    const int quantizedX = static_cast<int>(std::lround(x / 5.f));
    const int quantizedY = static_cast<int>(std::lround(y / 5.f));
    return brainLevelPrefix(brainActiveLevelId) + kind + "." +
        std::to_string(objectId) + "." + std::to_string(quantizedX) + "." +
        std::to_string(quantizedY);
}

inline void brainRecordLevelObject(GameObject* object) {
    if (!object) return;

    const int objectId = object->m_objectID;
    const int objectType = static_cast<int>(object->m_objectType);
    const float x = object->getPositionX();
    const float y = object->getPositionY();
    const int qx = static_cast<int>(std::lround(x / 5.f));
    const int qy = static_cast<int>(std::lround(y / 5.f));

    const std::string localKey = std::to_string(objectId) + "." +
        std::to_string(objectType) + "." + std::to_string(qx) + "." +
        std::to_string(qy);
    if (!brainLevelObjectKeys.insert(localKey).second) return;

    brainLevelObjects.push_back({objectId, objectType, x, y, qx, qy});

    if (!brainLearningEnabled || brainActiveLevelId == 0) return;
    auto* mod = geode::Mod::get();
    if (!mod) return;

    const auto key = brainObjectKey("layout", objectId, x, y) +
                     ".type." + std::to_string(objectType);
    if (!mod->getSavedValue<bool>(key + ".seen", false)) {
        mod->setSavedValue<bool>(key + ".seen", true);
        brainIncrement(brainLevelPrefix(brainActiveLevelId) + "layout-objects");
    }
}

inline std::uint64_t brainCurrentLevelLayoutCount() {
    return static_cast<std::uint64_t>(brainLevelObjects.size());
}

inline float brainNextHazardDistance(float playerX) {
    float best = std::numeric_limits<float>::infinity();

    for (const auto& object : brainLevelObjects) {
        const bool hazard =
            object.objectType == static_cast<int>(GameObjectType::Hazard) ||
            object.objectType == static_cast<int>(GameObjectType::AnimatedHazard);

        if (!hazard || object.x < playerX - 5.f) continue;

        best = std::min(best, std::max(0.f, object.x - playerX));
    }

    return best;
}

inline void brainRecordOrb(int mode, RingObject* ring) {
    if (!brainLearningEnabled || brainActiveLevelId == 0 || !ring) return;
    auto* mod = geode::Mod::get();
    if (!mod) return;

    const auto key = brainObjectKey("orb", ring->m_objectID,
                                    ring->getPositionX(), ring->getPositionY());
    if (!mod->getSavedValue<bool>(key + ".seen", false)) {
        mod->setSavedValue<bool>(key + ".seen", true);
        brainIncrement(brainLevelPrefix(brainActiveLevelId) + "orb-locations");
        brainIncrement("brain.objects.orb-locations");
    }

    brainIncrement(key + ".activations");
    brainIncrement(brainLevelPrefix(brainActiveLevelId) +
                    brainModeSuffix(mode) + "orb-activations");
}

inline void brainRecordEffect(int mode, EffectGameObject* object) {
    if (!brainLearningEnabled || brainActiveLevelId == 0 || !object) return;
    auto* mod = geode::Mod::get();
    if (!mod) return;

    const auto key = brainObjectKey("effect", object->m_objectID,
                                    object->getPositionX(), object->getPositionY());
    if (!mod->getSavedValue<bool>(key + ".seen", false)) {
        mod->setSavedValue<bool>(key + ".seen", true);
        brainIncrement(brainLevelPrefix(brainActiveLevelId) + "effect-locations");
        brainIncrement("brain.objects.effect-locations");
    }

    brainIncrement(key + ".activations");
    brainIncrement(brainLevelPrefix(brainActiveLevelId) +
                    brainModeSuffix(mode) + "effect-activations");
}

inline std::uint64_t brainTrackedLevels() {
    return brainReadCount("brain.levels.known");
}

inline std::uint64_t brainCurrentLevelSessions() {
    return brainActiveLevelId == 0 ? 0 :
        brainReadCount(brainLevelPrefix(brainActiveLevelId) + "sessions");
}

inline std::uint64_t brainCurrentLevelDeaths() {
    return brainActiveLevelId == 0 ? 0 :
        brainReadCount(brainLevelPrefix(brainActiveLevelId) + "deaths");
}

inline std::uint64_t brainCurrentLevelOrbs() {
    return brainActiveLevelId == 0 ? 0 :
        brainReadCount(brainLevelPrefix(brainActiveLevelId) + "orb-locations");
}

inline std::uint64_t brainCurrentLevelEffects() {
    return brainActiveLevelId == 0 ? 0 :
        brainReadCount(brainLevelPrefix(brainActiveLevelId) + "effect-locations");
}

inline int brainCurrentLevelBestX() {
    if (brainActiveLevelId == 0) return 0;
    auto* mod = geode::Mod::get();
    return mod ? mod->getSavedValue<int>(
        brainLevelPrefix(brainActiveLevelId) + "best-x", 0) : 0;
}

inline std::uint64_t brainTotalOrbLocations() {
    return brainReadCount("brain.objects.orb-locations");
}

inline std::uint64_t brainTotalEffectLocations() {
    return brainReadCount("brain.objects.effect-locations");
}

} // namespace virgo