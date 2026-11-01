$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$mainPath = Join-Path $PSScriptRoot 'src/main.cpp'
$sharedPath = Join-Path $PSScriptRoot 'src/VirgoShared.hpp'
if (-not (Test-Path $mainPath)) { throw 'Could not find src/main.cpp in the repository.' }
if (-not (Test-Path $sharedPath)) { throw 'Could not find src/VirgoShared.hpp in src. Add that file before building.' }

$shared = [System.IO.File]::ReadAllText($sharedPath)
foreach ($required in @('modeEnabled', 'modeFrames', 'predictionsByMode', 'safeDelaysByMode', 'deathsByMode', 'Cube', 'Wave', 'Ship', 'Ball', 'UFO', 'Robot', 'Spider', 'Swing')) {
    if (-not $shared.Contains($required)) { throw "src/VirgoShared.hpp is missing required setting: $required" }
}

$source = [System.IO.File]::ReadAllText($mainPath)
if ($source.Contains('// VIRGO MODE ENGINE PATCH v1')) {
    Write-Host 'VIRGO engine patch is already applied; leaving src/main.cpp unchanged.'
    exit 0
}

# Preserve CRLF/LF style but use LF while matching exact source anchors.
$newline = if ($source.Contains("`r`n")) { "`r`n" } else { "`n" }
$source = $source.Replace("`r`n", "`n")

function Replace-Once {
    param([string]$Text, [string]$Old, [string]$New, [string]$Label)
    # GitHub Actions checks out this Windows workspace with CRLF line endings.
    # Normalize the source AND here-string anchors before doing exact matches.
    $Text = $Text.Replace("`r`n", "`n")
    $Old = $Old.Replace("`r`n", "`n")
    $New = $New.Replace("`r`n", "`n")
    $count = ([regex]::Matches($Text, [regex]::Escape($Old))).Count
    if ($count -ne 1) {
        throw "Patch stopped safely at '$Label': expected one source match, found $count. No file was written."
    }
    return $Text.Replace($Old, $New)
}

$old = @'
#include <cstdint>

using namespace geode::prelude;
'@
$new = @'
#include <cstdint>
#include "VirgoShared.hpp"

using namespace geode::prelude;
'@
$source = Replace-Once $source $old $new 'include shared settings'

$old = @'
using namespace geode::prelude;
'@
$new = @'
using namespace geode::prelude;

// VIRGO MODE ENGINE PATCH v1
static int virgoModeIndex(PlayerObject* player) {
    if (!player) return virgo::Cube;
    if (player->m_isDart) return virgo::Wave;
    if (player->m_isShip) return virgo::Ship;
    if (player->m_isBall) return virgo::Ball;
    if (player->m_isBird) return virgo::UFO;
    if (player->m_isRobot) return virgo::Robot;
    if (player->m_isSpider) return virgo::Spider;
    if (player->m_isSwing) return virgo::Swing;
    return virgo::Cube;
}

static bool virgoModeIsEnabled(PlayerObject* player) {
    return player && virgo::modeEnabled[virgoModeIndex(player)];
}

static int virgoFrameLimitFor(PlayerObject* player, int fallback) {
    if (!player) return fallback;
    switch (virgoModeIndex(player)) {
        case virgo::Cube: return virgo::modeFrames[virgo::Cube];
        case virgo::Wave: return virgo::modeFrames[virgo::Wave];
        case virgo::Ship: return virgo::modeFrames[virgo::Ship];
        default: return fallback;
    }
}
'@
$source = Replace-Once $source $old $new 'insert gamemode helpers'

$old = @'
        m_simPlayer1 = createTrajectoryPlayer();
        m_simPlayer2 = createTrajectoryPlayer();
'@
$new = @'
        m_simPlayer1 = createTrajectoryPlayer();
        m_simPlayer2 = createTrajectoryPlayer();
        m_maxTicksNormal = std::clamp(virgo::modeFrames[virgo::Cube], 0, MAX_BUFFER_TICKS);
        m_maxTicksWave = std::clamp(virgo::modeFrames[virgo::Wave], 0, MAX_BUFFER_TICKS);
'@
$source = Replace-Once $source $old $new 'initialize Stage frame values'

$old = @'
        m_maxTicksNormal = normalTicks[m_presetIndex];
        m_maxTicksWave = waveTicks[m_presetIndex];
'@
$new = @'
        m_maxTicksNormal = normalTicks[m_presetIndex];
        m_maxTicksWave = waveTicks[m_presetIndex];
        virgo::modeFrames[virgo::Cube] = m_maxTicksNormal;
        virgo::modeFrames[virgo::Wave] = m_maxTicksWave;
        virgo::modeFrames[virgo::Ship] = m_maxTicksNormal;
'@
$source = Replace-Once $source $old $new 'sync F6 presets with Stage controls'

$old = @'
    int normalTicks() const { return m_maxTicksNormal; }
    int waveTicks() const { return m_maxTicksWave; }
'@
$new = @'
    int normalTicks() const { return virgo::modeFrames[virgo::Cube]; }
    int waveTicks() const { return virgo::modeFrames[virgo::Wave]; }
'@
$source = Replace-Once $source $old $new 'sync HUD with Stage values'

$old = @'
        // Real player died -> clear any pending buffer so respawn is 100% clean
        m_waitTicks = -1;
'@
$new = @'
        // Record real deaths per gamemode for future VIRGO learning telemetry.
        if (player) ++virgo::deathsByMode[virgoModeIndex(player)];
        // Real player died -> clear any pending buffer so respawn is 100% clean
        m_waitTicks = -1;
'@
$source = Replace-Once $source $old $new 'record deaths by mode'

$old = @'
        bool isContinuousMode = realP1->m_isDart || realP1->m_isBird || realP1->m_isSwing ||
                                (isDual && realP2 && (realP2->m_isDart || realP2->m_isBird || realP2->m_isSwing));
'@
$new = @'
        bool isContinuousMode = realP1->m_isDart || realP1->m_isBird || realP1->m_isSwing || realP1->m_isShip ||
                                (isDual && realP2 && (realP2->m_isDart || realP2->m_isBird || realP2->m_isSwing || realP2->m_isShip));
'@
$source = Replace-Once $source $old $new 'treat Ship as continuous input mode'

$old = @'
        // 1. NEVER touch Ship mode (or Dual with Ship)
        if (p1->m_isShip || (isDual && p2 && p2->m_isShip)) {
            m_waitTicks = -1;
            m_queuedOpposite = false;
            m_gameHolding = push;
            return true;
        }
'@
$new = @'
        // A disabled gamemode always receives raw input without predictive buffering.
        if (!virgoModeIsEnabled(p1) || (isDual && p2 && !virgoModeIsEnabled(p2))) {
            m_waitTicks = -1;
            m_queuedOpposite = false;
            m_gameHolding = push;
            return true;
        }
'@
$source = Replace-Once $source $old $new 'replace Ship bypass with per-mode switches'

$old = @'
        bool releaseAffectsPhysics = p1->m_isDart || p1->m_isRobot ||
                                     (isDual && p2 && (p2->m_isDart || p2->m_isRobot));
'@
$new = @'
        bool releaseAffectsPhysics = p1->m_isDart || p1->m_isRobot || p1->m_isShip ||
                                     (isDual && p2 && (p2->m_isDart || p2->m_isRobot || p2->m_isShip));
'@
$source = Replace-Once $source $old $new 'include Ship release behavior'

$old = @'
        int targetHorizon = isMiniWave ? 50 : (isWave ? 65 : 160);
        int maxDelayFrames = std::clamp(isWave ? m_maxTicksWave : m_maxTicksNormal, 0, MAX_BUFFER_TICKS);
'@
$new = @'
        int targetHorizon = isMiniWave ? 50 : (isWave ? 65 : 160);
        int modeLimit = virgoFrameLimitFor(p1, m_maxTicksNormal);
        if (isDual && p2) {
            modeLimit = std::min(modeLimit, virgoFrameLimitFor(p2, m_maxTicksNormal));
        }
        int maxDelayFrames = std::clamp(modeLimit, 0, MAX_BUFFER_TICKS);
'@
$source = Replace-Once $source $old $new 'use per-mode frame limits'

$old = @'
        // 4. Check Frame 0 (immediate input) across the full trajectory horizon
        int ticksFrame0 = getSurvivalTicks(pl, p1, p2, isDual, 0, push, targetHorizon);
'@
$new = @'
        const int telemetryMode = virgoModeIndex(p1);
        ++virgo::predictionsByMode[telemetryMode];

        // 4. Check Frame 0 (immediate input) across the full trajectory horizon
        int ticksFrame0 = getSurvivalTicks(pl, p1, p2, isDual, 0, push, targetHorizon);
'@
$source = Replace-Once $source $old $new 'record predictions by mode'

$old = @'
        if (bestDelay > 0) {
            // Throttle expensive console writes under repeated fast inputs.
'@
$new = @'
        if (bestDelay > 0) {
            ++virgo::safeDelaysByMode[telemetryMode];
            // Throttle expensive console writes under repeated fast inputs.
'@
$source = Replace-Once $source $old $new 'record selected safe delays'

$old = @'
        if (player->m_isShip || (pl->m_gameState.m_isDualMode && pl->m_player2 && pl->m_player2->m_isShip)) {
            m_waitTicks = -1;
            m_queuedOpposite = false;
            if (m_gameHolding != m_physicalHolding) {
                m_SimFireButton(pl, m_physicalHolding);
            }
            return;
        }
'@
$new = @'
        const bool dualMode = pl->m_gameState.m_isDualMode && pl->m_player2;
        if (!virgoModeIsEnabled(player) || (dualMode && !virgoModeIsEnabled(pl->m_player2))) {
            m_waitTicks = -1;
            m_queuedOpposite = false;
            if (m_gameHolding != m_physicalHolding) {
                m_SimFireButton(pl, m_physicalHolding);
            }
            return;
        }
'@
$source = Replace-Once $source $old $new 'respect mode switches during player updates'

# Validate all intended engine links before touching the source file.
foreach ($required in @('#include "VirgoShared.hpp"', 'virgo::modeEnabled', 'virgo::modeFrames', 'virgo::predictionsByMode', 'virgo::safeDelaysByMode', 'virgo::deathsByMode')) {
    if (-not $source.Contains($required)) { throw "Post-patch validation failed: missing $required. No file was written." }
}
if ($source.Contains('NEVER touch Ship mode')) { throw 'Old unconditional Ship bypass remains. No file was written.' }

$source = $source.Replace("`n", $newline)
[System.IO.File]::WriteAllText($mainPath, $source, [System.Text.UTF8Encoding]::new($false))
Write-Host 'VIRGO main.cpp engine patch applied successfully.'

