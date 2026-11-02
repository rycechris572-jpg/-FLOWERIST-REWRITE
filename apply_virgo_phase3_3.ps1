$ErrorActionPreference = "Stop"

function Read-TextLF([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) { throw "Required source file not found: $Path" }
    return ([System.IO.File]::ReadAllText((Resolve-Path -LiteralPath $Path))).Replace("`r`n", "`n").Replace("`r", "`n")
}

function Write-TextLF([string]$Path, [string]$Content) {
    $fullPath = [System.IO.Path]::GetFullPath($Path)
    [System.IO.File]::WriteAllText($fullPath, $Content, [System.Text.UTF8Encoding]::new($false))
}

function Replace-Once([string]$Source, [string]$Old, [string]$New, [string]$Label) {
    $Source = $Source.Replace("`r`n", "`n").Replace("`r", "`n")
    $Old = $Old.Replace("`r`n", "`n").Replace("`r", "`n")
    $New = $New.Replace("`r`n", "`n").Replace("`r", "`n")
    $oldCount = [regex]::Matches($Source, [regex]::Escape($Old)).Count
    if ($oldCount -eq 0) {
        if ([regex]::Matches($Source, [regex]::Escape($New)).Count -eq 1) {
            Write-Host "Already applied: $Label"
            return $Source
        }
        throw "Phase 3.3 stopped safely at ${Label}: expected one source match, found 0."
    }
    if ($oldCount -ne 1) {
        throw "Phase 3.3 stopped safely at ${Label}: expected one source match, found $oldCount."
    }
    Write-Host "Applying: $Label"
    return $Source.Replace($Old, $New)
}

function Replace-BrainTab([string]$Source, [string]$NewBlock) {
    $Source = $Source.Replace("`r`n", "`n").Replace("`r", "`n")
    if ($Source.Contains("AUTOPLAY PRESETS") -and $Source.Contains("EXECUTION: NOT ACTIVE YET")) {
        Write-Host "Already applied: Brain / Autoplay page"
        return $Source
    }

    $startMarker = "        } else if (m_selected == 5) {"
    $endMarker = "`n        }`n`n        updateNav();"
    $start = $Source.IndexOf($startMarker, [System.StringComparison]::Ordinal)
    if ($start -lt 0) { throw "Phase 3.3 stopped safely: Brain tab start marker not found." }

    $end = $Source.IndexOf($endMarker, $start, [System.StringComparison]::Ordinal)
    if ($end -lt 0) { throw "Phase 3.3 stopped safely: Brain tab end marker not found." }

    $closingBraceLength = "`n        }".Length
    return $Source.Substring(0, $start) + $NewBlock + $Source.Substring($end + $closingBraceLength)
}

# This script runs after apply_virgo_phase3_brain.ps1, so Phase 3.2's saved
# preference fields (brainLearningEnabled, autoPlayMode, autoPlayPreset,
# saveSettings) are already present before these edits are applied.
$main = Read-TextLF "src/main.cpp"
$ui = Read-TextLF "src/FloweristUI.cpp"

$brainHeader = @'
#pragma once

#include "VirgoShared.hpp"
#include <algorithm>
#include <cmath>
#include <string>

namespace virgo {

// Runtime identity for the level currently being played.
// Level ID zero is treated as untracked instead of merging unrelated levels.
inline int brainActiveLevelId = 0;
inline int brainActiveGameMode = Cube;

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
'@

# Shared helper is included after Phase 3.2's persistent settings implementation.
$main = Replace-Once $main '#include "VirgoShared.hpp"' ('#include "VirgoShared.hpp"' + "`n" + '#include "VirgoBrain.hpp"') "include the Phase 3.3 Brain data model"
$ui = Replace-Once $ui '#include "VirgoShared.hpp"' ('#include "VirgoShared.hpp"' + "`n" + '#include "VirgoBrain.hpp"') "include Brain data in the VIRGO GUI"

$main = Replace-Once $main `
    'if (player) virgo::recordDeath(virgoModeIndex(player));' `
    ('if (player) {' + "`n" + '            const int mode = virgoModeIndex(player);' + "`n" + '            virgo::recordDeath(mode);' + "`n" + '            virgo::brainRecordLevelDeath(mode, player->getPositionX());' + "`n" + '        }') `
    "store per-level deaths and best progress"

$main = Replace-Once $main `
    'if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;' `
    ('if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;' + "`n" + '        virgo::brainBeginLevel(level ? level->m_levelID : 0, virgoModeIndex(this->m_player1));') `
    "start or resume the selected level's Brain profile"

$main = Replace-Once $main `
    'if (PlayLayer::get() && !s_sim.isSimulating()) {' + "`n" + '            s_sim.setFrameDelta(dt);' + "`n" + '        }' `
    'if (PlayLayer::get() && !s_sim.isSimulating()) {' + "`n" + '            s_sim.setFrameDelta(dt);' + "`n" + '            virgo::brainRecordProgress(getPositionX());' + "`n" + '        }' `
    "record best level progress in bounded save intervals"

$main = Replace-Once $main `
    ('if (s_sim.isSimulating()) return;' + "`n" + '        EffectGameObject::triggerObject(p0, p1, p2);') `
    ('if (s_sim.isSimulating()) return;' + "`n" + '        auto* playLayer = PlayLayer::get();' + "`n" + '        virgo::brainRecordEffect(virgoModeIndex(playLayer ? playLayer->m_player1 : nullptr), this);' + "`n" + '        EffectGameObject::triggerObject(p0, p1, p2);') `
    "learn activated level effect objects"

$main = Replace-Once $main `
    ('if (s_sim.isSimulating()) return;' + "`n" + '        PlayerObject::ringJump(p0, p1);') `
    ('if (s_sim.isSimulating()) return;' + "`n" + '        virgo::brainRecordOrb(virgoModeIndex(this), p0);' + "`n" + '        PlayerObject::ringJump(p0, p1);') `
    "learn activated orbs and their positions"

$newBrainBlock = @'
        } else if (m_selected == 5) {
            const char* modeNames[virgo::ModeCount] = {
                "CUBE", "WAVE", "SHIP", "BALL", "UFO", "ROBOT", "SPIDER", "SWING"
            };

            // Per-level memory: all displayed values come from saved gameplay events.
            card(m_content, 205, 265, 220, 88, rgba(27, 29, 65, 255));
            text(m_content, "LEVEL MEMORY", 315, 292, 190, 13,
                 ccc3(220, 204, 255), "goldFont.fnt", .42f);
            const std::string levelLabel = virgo::brainActiveLevelId == 0
                ? "NO TRACKED LEVEL ID"
                : "LEVEL ID: " + std::to_string(virgo::brainActiveLevelId);
            text(m_content, levelLabel, 315, 273, 190, 11,
                 ccc3(190, 203, 241), "bigFont.fnt", .34f);
            const std::string sessionLine = "SESSIONS " +
                std::to_string(virgo::brainCurrentLevelSessions()) + "  DEATHS " +
                std::to_string(virgo::brainCurrentLevelDeaths());
            text(m_content, sessionLine, 315, 255, 194, 10,
                 ccc3(155, 179, 220), "bigFont.fnt", .31f);
            const std::string objectLine = "ORBS " +
                std::to_string(virgo::brainCurrentLevelOrbs()) + "  EFFECTS " +
                std::to_string(virgo::brainCurrentLevelEffects()) + "  BEST X " +
                std::to_string(virgo::brainCurrentLevelBestX());
            text(m_content, objectLine, 315, 239, 198, 10,
                 ccc3(155, 179, 220), "bigFont.fnt", .28f);
            text(m_content, "TRACKED LEVELS: " + std::to_string(virgo::brainTrackedLevels()),
                 315, 225, 190, 9, ccc3(124, 151, 205), "bigFont.fnt", .28f);

            // The mode and preset are real saved choices. The automatic input
            // executor stays inactive until its learning/decision stage is ready.
            card(m_content, 438, 265, 220, 88, rgba(35, 25, 64, 255));
            text(m_content, "AUTOPLAY CONFIG", 548, 292, 190, 13,
                 ccc3(220, 204, 255), "goldFont.fnt", .42f);
            button(m_content, virgo::brainLearningEnabled ? "BRAIN ON" : "BRAIN OFF",
                   493, 264, 86, 21, 400, true, false, virgo::brainLearningEnabled ? 3 : 2);
            button(m_content, virgo::autoPlayMode == 0 ? "NORMAL" : "PRACTICE",
                   602, 264, 94, 21, 401, true, false, 1);
            text(m_content, "EXECUTION: NOT ACTIVE YET", 548, 242, 190, 10,
                 ccc3(236, 190, 136), "bigFont.fnt", .30f);
            text(m_content, "SETUP IS SAVED", 548, 227, 190, 9,
                 ccc3(143, 164, 214), "bigFont.fnt", .28f);

            card(m_content, 205, 176, 453, 68, rgba(24, 27, 58, 255));
            text(m_content, "AUTOPLAY PRESETS", 430, 198, 250, 12,
                 ccc3(198, 178, 244), "bigFont.fnt", .38f);
            const char* presetNames[] = {"ZOINK", "TRICK", "DOGGIE", "NETTERMIND"};
            const float presetX[] = {260.f, 370.f, 480.f, 590.f};
            for (int i = 0; i < 4; ++i) {
                button(m_content, presetNames[i], presetX[i], 170, 92, 22,
                       410 + i, true, false, virgo::autoPlayPreset == i ? 3 : 0);
            }

            card(m_content, 205, 70, 453, 136, rgba(23, 26, 55, 255));
            text(m_content, "8 GAMEMODE MEMORY", 430, 124, 240, 13,
                 ccc3(220, 204, 255), "goldFont.fnt", .42f);
            text(m_content, "PRED", 377, 108, 65, 9, ccc3(135, 160, 211), "bigFont.fnt", .29f);
            text(m_content, "SAFE", 493, 108, 65, 9, ccc3(135, 160, 211), "bigFont.fnt", .29f);
            text(m_content, "DEATHS", 613, 108, 68, 9, ccc3(135, 160, 211), "bigFont.fnt", .29f);

            for (int mode = 0; mode < virgo::ModeCount; ++mode) {
                const auto index = static_cast<std::size_t>(mode);
                const float rowY = 94.f - static_cast<float>(mode) * 10.5f;
                const ccColor3B rowTint = (mode % 2 == 0)
                    ? ccc3(218, 221, 246) : ccc3(155, 174, 219);
                text(m_content, modeNames[mode], 249, rowY, 92, 9,
                     rowTint, "bigFont.fnt", .29f);
                text(m_content, std::to_string(virgo::predictionsByMode[index]), 377, rowY, 68, 9,
                     ccc3(219, 229, 255), "bigFont.fnt", .28f);
                text(m_content, std::to_string(virgo::safeDelaysByMode[index]), 493, rowY, 68, 9,
                     ccc3(180, 231, 217), "bigFont.fnt", .28f);
                text(m_content, std::to_string(virgo::deathsByMode[index]), 613, rowY, 68, 9,
                     ccc3(245, 190, 206), "bigFont.fnt", .28f);
            }
'@

# The original Brain branch ends immediately before updateNav().
# The replacement closes the page branch itself.
$newBrainBlock = $newBrainBlock + "        }"

$ui = Replace-BrainTab $ui $newBrainBlock

$actionAnchor = '        } else if (tag == 102) {'
$actionInsert = @'
        } else if (tag == 400) {
            virgo::brainLearningEnabled = !virgo::brainLearningEnabled;
            virgo::saveSettings();
            buildContent();
            return;
        } else if (tag == 401) {
            virgo::autoPlayMode = virgo::autoPlayMode == 0 ? 1 : 0;
            virgo::saveSettings();
            buildContent();
            return;
        } else if (tag >= 410 && tag < 414) {
            virgo::autoPlayPreset = tag - 410;
            virgo::saveSettings();
            buildContent();
            return;
        } else if (tag == 102) {
'@
$ui = Replace-Once $ui $actionAnchor $actionInsert "wire Brain learning, Autoplay mode and four preset buttons"

# Write only after every source anchor has passed validation.
Write-TextLF "src/VirgoBrain.hpp" $brainHeader
Write-TextLF "src/main.cpp" $main
Write-TextLF "src/FloweristUI.cpp" $ui
Write-Host "VIRGO Phase 3.3 Brain level-memory and Autoplay configuration patch applied."
