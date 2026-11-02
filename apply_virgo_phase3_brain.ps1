$ErrorActionPreference = "Stop"

function Read-TextLF([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Required source file not found: $Path"
    }
    return ([System.IO.File]::ReadAllText((Resolve-Path -LiteralPath $Path))).Replace("`r`n", "`n").Replace("`r", "`n")
}

function Write-TextLF([string]$Path, [string]$Content) {
    $fullPath = [System.IO.Path]::GetFullPath($Path)
    [System.IO.File]::WriteAllText($fullPath, $Content, [System.Text.UTF8Encoding]::new($false))
}

function Normalize-LF([string]$Text) {
    return $Text.Replace("`r`n", "`n").Replace("`r", "`n")
}

function Replace-Once([string]$Source, [string]$Old, [string]$New, [string]$Label) {
    # PowerShell here-strings preserve the script file's line endings. Normalize
    # both here-string anchors and source text so CRLF checkouts match LF anchors.
    $Source = Normalize-LF $Source
    $Old = Normalize-LF $Old
    $New = Normalize-LF $New
    $count = [regex]::Matches($Source, [regex]::Escape($Old)).Count
    if ($count -eq 0) {
        $alreadyCount = [regex]::Matches($Source, [regex]::Escape($New)).Count
        if ($alreadyCount -eq 1) {
            Write-Host "Already applied: $Label"
            return $Source
        }
        throw "Patch stopped safely at '$Label': expected one source match, found 0. No further files will be changed."
    }
    if ($count -ne 1) {
        throw "Patch stopped safely at '$Label': expected one source match, found $count. No further files will be changed."
    }
    Write-Host "Applying: $Label"
    return $Source.Replace($Old, $New)
}

# -----------------------------------------------------------------------------
# 1. Persistent Brain telemetry in VirgoShared.hpp
# -----------------------------------------------------------------------------
$sharedPath = "src/VirgoShared.hpp"
$shared = Read-TextLF $sharedPath

$oldInclude = @'
#pragma once
#include <array>
'@
$newInclude = @'
#pragma once
#include <Geode/Geode.hpp>
#include <array>
#include <string>
'@
$shared = Replace-Once $shared $oldInclude $newInclude "add Geode saved-data support"

$oldStats = @'
inline std::array<std::uint64_t, ModeCount> predictionsByMode = {};
inline std::array<std::uint64_t, ModeCount> safeDelaysByMode = {};
inline std::array<std::uint64_t, ModeCount> deathsByMode = {};
}
'@
$newStats = @'
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
'@
$shared = Replace-Once $shared $oldStats $newStats "install persistent Brain telemetry"

# -----------------------------------------------------------------------------
# 2. Feed actual engine events into saved Brain telemetry
# -----------------------------------------------------------------------------
$mainPath = "src/main.cpp"
$main = Read-TextLF $mainPath

$oldDeath = @'
        if (player) ++virgo::deathsByMode[virgoModeIndex(player)];
'@
$newDeath = @'
        if (player) virgo::recordDeath(virgoModeIndex(player));
'@
$main = Replace-Once $main $oldDeath $newDeath "record actual player deaths"

$oldPrediction = @'
        ++virgo::predictionsByMode[telemetryMode];
'@
$newPrediction = @'
        virgo::recordPrediction(telemetryMode);
'@
$main = Replace-Once $main $oldPrediction $newPrediction "record prediction attempts"

$oldSafeDelay = @'
            ++virgo::safeDelaysByMode[telemetryMode];
'@
$newSafeDelay = @'
            virgo::recordSafeDelay(telemetryMode);
'@
$main = Replace-Once $main $oldSafeDelay $newSafeDelay "record selected safe delays"

$oldLoaded = @'
$on_mod(Loaded) {
    log::info("Flowerist Rewrite loaded; console is opt-in from the LOG tab.");
}
'@
$newLoaded = @'
$on_mod(Loaded) {
    virgo::loadBrainData();
    log::info("VIRGO Brain initialized: persistent per-mode telemetry loaded.");
}
'@
$main = Replace-Once $main $oldLoaded $newLoaded "load saved Brain data on mod startup"

# -----------------------------------------------------------------------------
# 3. Replace the Competitive page with the Brain / Autoplay dashboard
# -----------------------------------------------------------------------------
$uiPath = "src/FloweristUI.cpp"
$ui = Read-TextLF $uiPath

$oldTabs = @'
const char* TAB_NAMES[TAB_COUNT] = {"LOGS", "STAGES", "MODES", "ASSISTS", "OPTIMIZATION", "COMPETITIVE"};
'@
$newTabs = @'
const char* TAB_NAMES[TAB_COUNT] = {"LOGS", "STAGES", "MODES", "ASSISTS", "OPTIMIZATION", "BRAIN / AUTO"};
'@
$ui = Replace-Once $ui $oldTabs $newTabs "replace Competitive navigation tab"

$oldTabInfo = @'
const char* TAB_INFO[TAB_COUNT] = {
    "System status and recent activity",
    "Choose a stage and tune Cube, Wave and Ship",
    "Choose which gamemodes use Predictive Input Buffer",
    "See the active assistance systems",
    "Tune the interface and performance",
    "A profile for rule-conscious play"
};
'@
$newTabInfo = @'
const char* TAB_INFO[TAB_COUNT] = {
    "System status and recent activity",
    "Choose a stage and tune Cube, Wave and Ship",
    "Choose which gamemodes use Predictive Input Buffer",
    "See the active assistance systems",
    "Tune the interface and performance",
    "Persistent learning records and level-aware autoplay"
};
'@
$ui = Replace-Once $ui $oldTabInfo $newTabInfo "update Brain navigation description"

$oldBuildStart = @'
    void buildContent() {
        clearPageHits();
'@
$newBuildStart = @'
    void buildContent() {
        virgo::loadBrainData();
        clearPageHits();
'@
$ui = Replace-Once $ui $oldBuildStart $newBuildStart "load saved Brain records before drawing the dashboard"

$oldSectionTitle = @'
        sectionTitle(TAB_NAMES[m_selected], TAB_INFO[m_selected]);
'@
$newSectionTitle = @'
        sectionTitle(m_selected == 5 ? "BRAIN / AUTOPLAY" : TAB_NAMES[m_selected], TAB_INFO[m_selected]);
'@
$ui = Replace-Once $ui $oldSectionTitle $newSectionTitle "show full Brain and Autoplay page title"

$oldBrainPage = @'
        } else if (m_selected == 5) {
            card(m_content, 204, 163, 460, 175, rgba(23, 25, 54, 255));
            text(m_content, "COMPETITIVE PROFILE", 432, 295, 360, 23, ccc3(221, 202, 255), "goldFont.fnt");
            text(m_content, "Review the rules of each event before enabling assistance.",
                 432, 253, 410, 30, ccc3(155, 172, 216));
            rect(m_content, 231, 226, 402, 1, rgba(95, 108, 159, 130), 6);
            text(m_content, "This page is a reminder to check event rules, not an anti-cheat bypass.",
                 432, 203, 410, 32, ccc3(155, 172, 216));
            text(m_content, "MANUAL PROFILE", 432, 175, 200, 16, ccc3(238, 190, 122));
        }
'@
$newBrainPage = @'
        } else if (m_selected == 5) {
            // These are real, persisted engine counters. Grades and IQ remain hidden
            // until VIRGO has enough level-specific evidence to calculate them honestly.
            const char* modeNames[virgo::ModeCount] = {
                "CUBE", "WAVE", "SHIP", "BALL", "UFO", "ROBOT", "SPIDER", "SWING"
            };
            card(m_content, 205, 266, 220, 67, rgba(27, 29, 65, 255));
            card(m_content, 438, 266, 220, 67, rgba(37, 25, 65, 255));
            text(m_content, "PREDICTION ATTEMPTS", 315, 314, 190, 13,
                 ccc3(170, 170, 235), "bigFont.fnt", .34f);
            text(m_content, std::to_string(virgo::totalPredictions()), 315, 289, 190, 22,
                 ccc3(239, 226, 255), "goldFont.fnt", .66f);
            text(m_content, "ALL MODES - SAVED DATA", 315, 273, 180, 10,
                 ccc3(123, 150, 204), "bigFont.fnt", .30f);
            text(m_content, "SAFE DELAYS SELECTED", 548, 314, 190, 13,
                 ccc3(170, 170, 235), "bigFont.fnt", .34f);
            text(m_content, std::to_string(virgo::totalSafeDelays()), 548, 289, 170, 22,
                 ccc3(239, 226, 255), "goldFont.fnt", .66f);
            text(m_content, "ALL MODES - SAVED DATA", 548, 273, 180, 10,
                 ccc3(123, 150, 204), "bigFont.fnt", .30f);

            card(m_content, 205, 65, 453, 187, rgba(23, 26, 55, 255));
            text(m_content, "MODE MEMORY", 430, 231, 220, 15,
                 ccc3(220, 204, 255), "goldFont.fnt", .48f);
            text(m_content, "PRED", 377, 210, 65, 10, ccc3(135, 160, 211), "bigFont.fnt", .31f);
            text(m_content, "DELAY", 493, 210, 65, 10, ccc3(135, 160, 211), "bigFont.fnt", .31f);
            text(m_content, "DEATHS", 613, 210, 68, 10, ccc3(135, 160, 211), "bigFont.fnt", .31f);

            for (int mode = 0; mode < virgo::ModeCount; ++mode) {
                const auto index = static_cast<std::size_t>(mode);
                const float rowY = 190.f - static_cast<float>(mode) * 16.f;
                const ccColor3B rowTint = (mode % 2 == 0)
                    ? ccc3(218, 221, 246) : ccc3(155, 174, 219);
                text(m_content, modeNames[mode], 249, rowY, 92, 11,
                     rowTint, "bigFont.fnt", .34f);
                text(m_content, std::to_string(virgo::predictionsByMode[index]), 377, rowY, 68, 11,
                     ccc3(219, 229, 255), "bigFont.fnt", .33f);
                text(m_content, std::to_string(virgo::safeDelaysByMode[index]), 493, rowY, 68, 11,
                     ccc3(180, 231, 217), "bigFont.fnt", .33f);
                text(m_content, std::to_string(virgo::deathsByMode[index]), 613, rowY, 68, 11,
                     ccc3(245, 190, 206), "bigFont.fnt", .33f);
            }
            text(m_content, "LEVEL LEARNING + AUTOPLAY: NEXT BRAIN MILESTONE",
                 430, 44, 440, 12, ccc3(184, 151, 239), "bigFont.fnt", .34f);
        }
'@
$ui = Replace-Once $ui $oldBrainPage $newBrainPage "replace Competitive page with Brain and Autoplay dashboard"

# All source anchors have passed. Only now write the edited files.
Write-TextLF $sharedPath $shared
Write-TextLF $mainPath $main
Write-TextLF $uiPath $ui

Write-Host "VIRGO Phase 3 Brain foundation patch applied."
