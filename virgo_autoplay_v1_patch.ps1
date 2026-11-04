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
    # Normalize line endings before matching.
    $Source = $Source.Replace("`r`n", "`n").Replace("`r", "`n")
    $Old = $Old.Replace("`r`n", "`n").Replace("`r", "`n")
    $New = $New.Replace("`r`n", "`n").Replace("`r", "`n")

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
            object.objectType == static_cast<int>(GameObjectType::Hazard) ||
            object.objectType == static_cast<int>(GameObjectType::AnimatedHazard);

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

$quitSignature = '    void quit() {'
$quitCount = ([regex]::Matches(
    $main, [regex]::Escape($quitSignature)
)).Count

if ($quitCount -ne 1) {
    throw "Expected one quit() function; found $quitCount. No files were written."
}

$quitStart = $main.IndexOf($quitSignature)
$quitOpen = $main.IndexOf('{', $quitStart)
$depth = 0
$quitEnd = -1

for ($i = $quitOpen; $i -lt $main.Length; $i++) {
    if ($main[$i] -eq '{') {
        $depth++
    }
    elseif ($main[$i] -eq '}') {
        $depth--
        if ($depth -eq 0) {
            $quitEnd = $i
            break
        }
    }
}

if ($quitEnd -lt 0) {
    throw "Could not locate the end of quit(). No files were written."
}

$resetCode = @'
        m_autoDecisionCooldown = 0;
        m_autoTapReleaseTicks = 0;
        m_autoWasActive = false;
'@

$quitBody = $main.Substring($quitStart, $quitEnd - $quitStart)
if (-not $quitBody.Contains('m_autoDecisionCooldown = 0;')) {
    $quitLineStart = $main.LastIndexOf("`n", $quitEnd) + 1
    $main = $main.Insert($quitLineStart, $resetCode)
}
$new = @'
        PlayLayer* pilotLayer = PlayLayer::get();
        const bool pilotPracticeBlocked =
            virgo::autoPlayEnabled && virgo::autoPlayMode == 1 &&
            (!pilotLayer || !pilotLayer->m_isPracticeMode);
        const bool pilotTwoPlayerBlocked =
            virgo::autoPlayEnabled && pilotLayer && pilotLayer->m_level &&
            pilotLayer->m_level->m_twoPlayerMode;
        const bool pilotOwnsInput =
            virgo::autoPlayEnabled &&
            !virgo::brainLevelObjects.empty() &&
            !pilotPracticeBlocked &&
            !pilotTwoPlayerBlocked;
        if (pilotOwnsInput) return;
        if (!m_enabled) return;
'@

$main = Replace-Once $main `
    '        if (!m_enabled) return;' `
    $new `
    "Pause input buffer only while autopilot actually owns input"

$deathSignature = '    bool checkSimulationDeath(PlayerObject* player) {'

$deathCount = ([regex]::Matches(
    $main, [regex]::Escape($deathSignature)
)).Count

if ($deathCount -ne 1) {
    throw "Expected one checkSimulationDeath() function; found $deathCount. No files were written."
}

$deathStart = $main.IndexOf($deathSignature)
$deathOpen = $main.IndexOf('{', $deathStart)
$depth = 0
$deathEnd = -1

for ($i = $deathOpen; $i -lt $main.Length; $i++) {
    if ($main[$i] -eq '{') {
        $depth++
    }
    elseif ($main[$i] -eq '}') {
        $depth--
        if ($depth -eq 0) {
            $deathEnd = $i
            break
        }
    }
}

if ($deathEnd -lt 0) {
    throw "Could not locate the end of checkSimulationDeath(). No files were written."
}

$deathBody = $main.Substring($deathStart, $deathEnd - $deathStart)

if (-not $deathBody.Contains('m_autoTapReleaseTicks = 0;')) {
    $returnCount = ([regex]::Matches(
        $deathBody, [regex]::Escape('return false;')
    )).Count

    if ($returnCount -ne 1) {
        throw "Expected one return false in death handler; found $returnCount. No files were written."
    }

    $returnIndex = $deathStart + $deathBody.IndexOf('return false;')
    $returnLineStart = $main.LastIndexOf("`n", $returnIndex) + 1

    $resetCode = @'
        m_autoTapReleaseTicks = 0;
        m_autoDecisionCooldown = 8;
'@

    $main = $main.Insert($returnLineStart, $resetCode)
}

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
    // Experimental v1 pilot. A heuristic controller, not a full-level solver.
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

        // The four selectable presets now alter the pilot's actual scan profile.
        // These are VIRGO tuning profiles, not claims to reproduce a player's inputs.
        const int selectedPreset = std::clamp(virgo::autoPlayPreset, -1, 3);
        int lookaheadTicks = 32;
        int decisionCooldown = 24;
        int minImprovement = 3;
        float triggerDistance = 115.f;

        switch (selectedPreset) {
            case 0: // ZOINK: quicker, more responsive decisions
                lookaheadTicks = 24;
                decisionCooldown = 18;
                minImprovement = 2;
                triggerDistance = 100.f;
                break;
            case 1: // TRICK: balanced scan profile
                lookaheadTicks = 32;
                decisionCooldown = 24;
                minImprovement = 3;
                triggerDistance = 115.f;
                break;
            case 2: // DOGGIE: longer lookahead, more conservative changes
                lookaheadTicks = 40;
                decisionCooldown = 30;
                minImprovement = 5;
                triggerDistance = 140.f;
                break;
            case 3: // NETTERMIND: longer scan with a moderate threshold
                lookaheadTicks = 36;
                decisionCooldown = 27;
                minImprovement = 3;
                triggerDistance = 130.f;
                break;
            default:
                break;
        }

        // Throttle every decision attempt, including decisions that do not change
        // the button state. This prevents two full simulations from running each frame.
        m_autoDecisionCooldown = decisionCooldown;

        const float hazardDistance =
            virgo::brainNextHazardDistance(player->getPositionX());
        const int mode = virgoModeIndex(player);

        const bool impulseMode =
            mode == virgo::Cube ||
            mode == virgo::Ball ||
            mode == virgo::UFO ||
            mode == virgo::Spider;

        if (!std::isfinite(hazardDistance) || hazardDistance > triggerDistance) {
            if (!impulseMode && m_gameHolding) {
                m_SimFireButton(pl, false);
            }
            return;
        }

        if (impulseMode) {
            if (mode == virgo::Cube && !player->m_isOnGround) return;
            if (m_gameHolding) return;

            const int coast = getSurvivalTicks(
                pl, player, pl->m_player2,
                pl->m_gameState.m_isDualMode && pl->m_player2 != nullptr,
                0, false, lookaheadTicks
            );

            const int tap = getSurvivalTicks(
                pl, player, pl->m_player2,
                pl->m_gameState.m_isDualMode && pl->m_player2 != nullptr,
                0, true, lookaheadTicks
            );

            if (tap >= coast + minImprovement) {
                m_SimFireButton(pl, true);
                m_autoTapReleaseTicks = 1;
            }
            return;
        }

        const bool dual =
            pl->m_gameState.m_isDualMode && pl->m_player2 != nullptr;
        PlayerObject* p2 = dual ? pl->m_player2 : nullptr;

        const int keepScore = getSurvivalTicks(
            pl, player, p2, dual, 0, m_gameHolding, lookaheadTicks
        );

        const int switchScore = getSurvivalTicks(
            pl, player, p2, dual, 0, !m_gameHolding, lookaheadTicks
        );

        if (switchScore >= keepScore + minImprovement) {
            m_SimFireButton(pl, !m_gameHolding);
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

# Find PlayLayer::onQuit() without an exact-text anchor.
$quitSignature = '    void onQuit() {'
$onQuitCount = ([regex]::Matches($main, [regex]::Escape($quitSignature))).Count

if ($onQuitCount -ne 1) {
    throw "Expected one PlayLayer onQuit() method; found $onQuitCount. No files were written."
}

$onQuitStart = $main.IndexOf($quitSignature)
$onQuitOpen = $main.IndexOf('{', $onQuitStart)
$depth = 0
$onQuitEnd = -1

for ($i = $onQuitOpen; $i -lt $main.Length; $i++) {
    if ($main[$i] -eq '{') {
        $depth++
    }
    elseif ($main[$i] -eq '}') {
        $depth--
        if ($depth -eq 0) {
            $onQuitEnd = $i
            break
        }
    }
}

if ($onQuitEnd -lt 0) {
    throw "Could not locate the end of PlayLayer onQuit(). No files were written."
}

$onQuitBody = $main.Substring(
    $onQuitStart,
    $onQuitEnd - $onQuitStart
)

if (-not $onQuitBody.Contains('virgo::brainLevelObjects.clear();')) {
    $main = $main.Insert(
        $onQuitEnd,
        "`n        virgo::brainLevelObjects.clear();`n        virgo::brainLevelObjectKeys.clear();"
    )
}

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
    'if (pilotOwnsInput) return;',
    'm_autoDecisionCooldown = decisionCooldown;',
    'case 0: // ZOINK',
    'case 1: // TRICK',
    'case 2: // DOGGIE',
    'case 3: // NETTERMIND'
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
