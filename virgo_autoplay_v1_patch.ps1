$ErrorActionPreference = "Stop"

function Read-TextLF([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Required file not found: $Path"
    }
    return ([System.IO.File]::ReadAllText((Resolve-Path -LiteralPath $Path))).Replace("`r`n", "`n").Replace("`r", "`n")
}

function Write-TextLF([string]$Path, [string]$Content) {
    [System.IO.File]::WriteAllText(
        [System.IO.Path]::GetFullPath($Path),
        $Content,
        [System.Text.UTF8Encoding]::new($false)
    )
}

function Replace-Once([string]$Source, [string]$Old, [string]$New, [string]$Label) {
    if ($Source.Contains($New)) { return $Source }
    $count = ([regex]::Matches($Source, [regex]::Escape($Old))).Count
    if ($count -ne 1) {
        throw "Expected one patch anchor for '$Label'; found $count. No files were written."
    }
    return $Source.Replace($Old, $New)
}

$brain = Read-TextLF "src/VirgoBrain.hpp"
$main = Read-TextLF "src/main.cpp"
$ui = Read-TextLF "src/FloweristUI.cpp"

# ------------------------------------------------------------
# BRAIN: level object map
# ------------------------------------------------------------

$brain = Replace-Once $brain '#include <string>' "#include <string>`n#include <vector>`n#include <unordered_set>`n#include <limits>" "Brain STL includes"

$new = @'
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
'@

$brain = Replace-Once $brain 'inline int brainActiveGameMode = Cube;' $new "Brain level object model"

$brain = Replace-Once $brain `
    '    brainActiveGameMode = (mode >= 0 && mode < ModeCount) ? mode : Cube;' `
    "    brainActiveGameMode = (mode >= 0 && mode < ModeCount) ? mode : Cube;`n    brainLevelObjects.clear();`n    brainLevelObjectKeys.clear();" `
    "Clear level object map"

$new = @'
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
            object.objectType == static_cast<int>(GameObjectType::Solid) ||
            object.objectType == static_cast<int>(GameObjectType::Hazard) ||
            object.objectType == static_cast<int>(GameObjectType::AnimatedHazard) ||
            object.objectType == static_cast<int>(GameObjectType::CollisionObject) ||
            object.objectType == static_cast<int>(GameObjectType::Slope);

        if (!hazard || object.x < playerX - 5.f) continue;

        best = std::min(best, std::max(0.f, object.x - playerX));
    }

    return best;
}

inline void brainRecordOrb(int mode, RingObject* ring) {
'@

$brain = Replace-Once $brain `
    'inline void brainRecordOrb(int mode, RingObject* ring) {' `
    $new `
    "Brain object map and hazard lookup"

# ------------------------------------------------------------
# MAIN: runtime state and simulation-driven pilot
# ------------------------------------------------------------

$main = Replace-Once $main `
    '#include <algorithm>' `
    "#include <algorithm>`n#include <cmath>" `
    "Math include"

$new = @'
    unsigned int m_suppressedConsoleMessages = 0;

    // Experimental simulation-driven autopilot state.
    int m_autoDecisionCooldown = 0;
    int m_autoTapReleaseTicks = 0;
    bool m_autoWasActive = false;
'@

$main = Replace-Once $main `
    '    unsigned int m_suppressedConsoleMessages = 0;' `
    $new `
    "Autoplay runtime state"

# FIXED: match only the end of quit(), without relying on the next comment.
$old = @'
        m_rapidInputStreak = 0;
    }
'@

$new = @'
        m_rapidInputStreak = 0;
        m_autoDecisionCooldown = 0;
        m_autoTapReleaseTicks = 0;
        m_autoWasActive = false;
    }
'@

$main = Replace-Once $main $old $new "Autoplay quit state"

$main = Replace-Once $main `
    '        if (!m_enabled) return;' `
    "        if (virgo::autoPlayEnabled) return;`n        if (!m_enabled) return;" `
    "Pause input buffer while autopilot owns controls"

$oldDeath = @'
        m_waitTicks = -1;
        m_queuedOpposite = false;
        m_gameHolding = false;
        m_rapidInputStreak = 0;
        return false;
'@

$newDeath = @'
        m_waitTicks = -1;
        m_queuedOpposite = false;
        m_gameHolding = false;
        m_rapidInputStreak = 0;
        m_autoTapReleaseTicks = 0;
        m_autoDecisionCooldown = 8;
        return false;
'@

$main = Replace-Once $main $oldDeath $newDeath "Reset pilot state after player death"

$new = @'
        m_physicalHolding = push;
        PlayLayer* activeLayer = PlayLayer::get();
        const bool practiceBlocked = virgo::autoPlayEnabled && virgo::autoPlayMode == 1 &&
                                     (!activeLayer || !activeLayer->m_isPracticeMode);
        const bool twoPlayerBlocked = virgo::autoPlayEnabled && activeLayer && activeLayer->m_level &&
                                      activeLayer->m_level->m_twoPlayerMode;
        const bool mapReady = !virgo::brainLevelObjects.empty();
        if (virgo::autoPlayEnabled && mapReady && !practiceBlocked && !twoPlayerBlocked) return false;
'@

$main = Replace-Once $main `
    '        m_physicalHolding = push;' `
    $new `
    "Block manual input only while the pilot can operate"

$new = @'
    // Experimental v1 pilot. This is a first heuristic controller,
    // not a completed or verified full-level solver.
    void autoplayStep(PlayerObject* player) {
        PlayLayer* pl = PlayLayer::get();
        if (!pl || !player || player != pl->m_player1 || m_simulating) return;

        const bool practiceBlocked = virgo::autoPlayEnabled &&
                                     virgo::autoPlayMode == 1 &&
                                     !pl->m_isPracticeMode;
        const bool twoPlayerBlocked = pl->m_level &&
                                      pl->m_level->m_twoPlayerMode;

        const bool canOperate =
            virgo::autoPlayEnabled &&
            !practiceBlocked &&
            !twoPlayerBlocked &&
            !player->m_isDead &&
            !virgo::brainLevelObjects.empty();

        if (!canOperate) {
            if (m_autoWasActive) {
                if (m_gameHolding) m_SimFireButton(pl, false);
                m_physicalHolding = false;
            }

            m_autoWasActive = false;
            m_autoDecisionCooldown = 0;
            m_autoTapReleaseTicks = 0;
            return;
        }

        if (!m_autoWasActive) {
            if (m_gameHolding) m_SimFireButton(pl, false);
            m_physicalHolding = false;
            m_waitTicks = -1;
            m_queuedOpposite = false;
        }

        m_autoWasActive = true;

        if (m_autoTapReleaseTicks > 0) {
            --m_autoTapReleaseTicks;

            if (m_autoTapReleaseTicks == 0 && m_gameHolding) {
                m_SimFireButton(pl, false);
            }

            return;
        }

        if (m_autoDecisionCooldown > 0) {
            --m_autoDecisionCooldown;
            return;
        }

        const float hazardDistance =
            virgo::brainNextHazardDistance(player->getPositionX());

        const int mode = virgoModeIndex(player);

        const bool impulseMode =
            mode == virgo::Cube ||
            mode == virgo::Ball ||
            mode == virgo::UFO ||
            mode == virgo::Spider;

        if (!std::isfinite(hazardDistance) || hazardDistance > 115.f) {
            if (!impulseMode && m_gameHolding) {
                m_SimFireButton(pl, false);
            }
            return;
        }

        const bool dual =
            pl->m_gameState.m_isDualMode && pl->m_player2 != nullptr;

        PlayerObject* p2 = dual ? pl->m_player2 : nullptr;

        constexpr int LOOKAHEAD_TICKS = 40;
        constexpr int MIN_IMPROVEMENT = 3;

        if (impulseMode) {
            if (mode == virgo::Cube && !player->m_isOnGround) return;
            if (m_gameHolding) return;

            const int coast = getSurvivalTicks(
                pl, player, p2, dual, 0, false, LOOKAHEAD_TICKS
            );

            const int tap = getSurvivalTicks(
                pl, player, p2, dual, 0, true, LOOKAHEAD_TICKS
            );

            if (tap >= coast + MIN_IMPROVEMENT) {
                m_SimFireButton(pl, true);
                m_autoTapReleaseTicks = 1;
                m_autoDecisionCooldown = 8;
            }

            return;
        }

        const int keepScore = getSurvivalTicks(
            pl, player, p2, dual, 0, m_gameHolding, LOOKAHEAD_TICKS
        );

        const int switchScore = getSurvivalTicks(
            pl, player, p2, dual, 0, !m_gameHolding, LOOKAHEAD_TICKS
        );

        if (switchScore >= keepScore + MIN_IMPROVEMENT) {
            m_SimFireButton(pl, !m_gameHolding);
            m_autoDecisionCooldown = 8;
        }
    }

    void m_SimFireButton(PlayLayer* pl, bool push) {
'@

$main = Replace-Once $main `
    '    void m_SimFireButton(PlayLayer* pl, bool push) {' `
    $new `
    "Simulation-driven autoplay controller"

$new = @'
    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();

        virgo::brainLevelObjects.clear();
        virgo::brainLevelObjectKeys.clear();

        if (m_objects) {
            for (auto* object : CCArrayExt<GameObject>(m_objects)) {
                virgo::brainRecordLevelObject(object);
            }
        }

        log::info(
            "VIRGO indexed {} level objects for Brain/Autoplay.",
            virgo::brainCurrentLevelLayoutCount()
        );
    }

    void destroyPlayer(PlayerObject* player, GameObject* gameObject) {
'@

$main = Replace-Once $main `
    '    void destroyPlayer(PlayerObject* player, GameObject* gameObject) {' `
    $new `
    "PlayLayer level object scan"

$main = Replace-Once $main `
    '            virgo::brainRecordProgress(getPositionX());' `
    "            virgo::brainRecordProgress(getPositionX());`n            s_sim.autoplayStep(this);" `
    "Call autopilot after real player update"

$old = @'
        s_sim.quit();
    }
};

class $modify(BufEditorLayer, LevelEditorLayer) {
'@

$new = @'
        s_sim.quit();
        virgo::brainLevelObjects.clear();
        virgo::brainLevelObjectKeys.clear();
    }
};

class $modify(BufEditorLayer, LevelEditorLayer) {
'@

$main = Replace-Once $main $old $new "Clear level object map on quit"

# ------------------------------------------------------------
# UI: autoplay control
# ------------------------------------------------------------

$old = @'
            text(m_content, "EXECUTION: NOT ACTIVE YET", 548, 245, 194, 12,
                 ccc3(213, 171, 132), "chatFont.fnt", .34f);
            text(m_content, "SETUP IS SAVED", 548, 225, 194, 11,
                 ccc3(157, 143, 183), "chatFont.fnt", .33f);
'@

$new = @'
            button(m_content,
                   virgo::autoPlayEnabled ? "AUTOPLAY ON" : "AUTOPLAY OFF",
                   548, 241, 136, 23, 402, true, false,
                   virgo::autoPlayEnabled ? 1 : 2);

            const std::string mapStatus =
                virgo::brainCurrentLevelLayoutCount() > 0
                ? "MAP READY: " +
                    std::to_string(virgo::brainCurrentLevelLayoutCount()) +
                    " OBJECTS"
                : "ENTER A LEVEL TO SCAN ITS MAP";

            text(m_content, mapStatus, 548, 219, 194, 11,
                 ccc3(157, 143, 183), "chatFont.fnt", .31f);
'@

$ui = Replace-Once $ui $old $new "Autoplay UI control"

$new = @'
        } else if (tag == 402) {
            virgo::autoPlayEnabled = !virgo::autoPlayEnabled;
            virgo::saveSettings();
            buildContent();
            return;
        } else if (tag >= 410 && tag < 414) {
'@

$ui = Replace-Once $ui `
    '        } else if (tag >= 410 && tag < 414) {' `
    $new `
    "Autoplay UI action"

# ------------------------------------------------------------
# VALIDATION: check all files before writing any changes
# ------------------------------------------------------------

foreach ($item in @(
    'inline void brainRecordLevelObject(GameObject* object)',
    'inline float brainNextHazardDistance(float playerX)',
    'inline std::uint64_t brainCurrentLevelLayoutCount()',
    'brainLevelObjects.clear();'
)) {
    if (-not $brain.Contains($item)) {
        throw "Brain Autoplay integration missing: $item"
    }
}

foreach ($item in @(
    'void autoplayStep(PlayerObject* player)',
    's_sim.autoplayStep(this);',
    'void setupHasCompleted()',
    'if (virgo::autoPlayEnabled) return;'
)) {
    if (-not $main.Contains($item)) {
        throw "Main Autoplay integration missing: $item"
    }
}

foreach ($item in @(
    'AUTOPLAY ON',
    'tag == 402',
    'brainCurrentLevelLayoutCount()'
)) {
    if (-not $ui.Contains($item)) {
        throw "UI Autoplay integration missing: $item"
    }
}

# Write only after every replacement and validation succeeds.
Write-TextLF "src/VirgoBrain.hpp" $brain
Write-TextLF "src/main.cpp" $main
Write-TextLF "src/FloweristUI.cpp" $ui

Write-Host "VIRGO Autoplay v1 patch applied."
