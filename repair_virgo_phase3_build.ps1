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

function Find-FunctionEnd([string]$Source, [int]$Start) {
    $open = $Source.IndexOf("{", $Start)
    if ($open -lt 0) { throw "Could not find function body opening brace." }

    $depth = 0
    for ($i = $open; $i -lt $Source.Length; $i++) {
        if ($Source[$i] -eq '{') {
            $depth++
        } elseif ($Source[$i] -eq '}') {
            $depth--
            if ($depth -eq 0) { return ($i + 1) }
        }
    }
    throw "Could not find matching function closing brace."
}

function Count-Exact([string]$Source, [string]$Needle) {
    return [regex]::Matches($Source, [regex]::Escape($Needle)).Count
}

# Read and repair in memory first. Nothing is written until every check passes.
$shared = Read-TextLF "src/VirgoShared.hpp"
$main = Read-TextLF "src/main.cpp"
$ui = Read-TextLF "src/FloweristUI.cpp"
$brain = Read-TextLF "src/VirgoBrain.hpp"

# Phase 3.2's original patcher could append the same preference declarations
# and load/save functions on every workflow run. Keep exactly one copy.
$prefMarker = "// Persistent user preferences."
$brainMarker = "// Brain foundation: real telemetry, persisted by Geode between game sessions."
$prefStart = $shared.IndexOf($prefMarker, [System.StringComparison]::Ordinal)
$brainStart = $shared.IndexOf($brainMarker, [System.StringComparison]::Ordinal)
if ($prefStart -lt 0 -or $brainStart -le $prefStart) {
    throw "Could not locate the persistent preference section in VirgoShared.hpp."
}
$prefEndMarker = "inline int autoPlayPreset = -1;"
$prefEndIndex = $shared.IndexOf($prefEndMarker, $prefStart, [System.StringComparison]::Ordinal)
if ($prefEndIndex -lt 0 -or $prefEndIndex -ge $brainStart) {
    throw "Could not locate the end of the persistent preference declarations."
}
$prefEnd = $prefEndIndex + $prefEndMarker.Length
$onePreferenceBlock = $shared.Substring($prefStart, $prefEnd - $prefStart)
$shared = $shared.Substring(0, $prefStart) + $onePreferenceBlock + "`n`n" + $shared.Substring($brainStart)

$loadMarker = "inline void loadSettings() {"
$saveMarker = "inline void saveSettings() {"
$dataMarker = "inline void loadBrainData() {"
$loadStart = $shared.IndexOf($loadMarker, [System.StringComparison]::Ordinal)
if ($loadStart -lt 0) { throw "loadSettings() was not found in VirgoShared.hpp." }
$loadEnd = Find-FunctionEnd $shared $loadStart
$saveStart = $shared.IndexOf($saveMarker, $loadEnd, [System.StringComparison]::Ordinal)
if ($saveStart -lt 0) { throw "saveSettings() was not found after loadSettings()." }
$saveEnd = Find-FunctionEnd $shared $saveStart
$dataStart = $shared.IndexOf($dataMarker, $saveEnd, [System.StringComparison]::Ordinal)
if ($dataStart -lt 0) { throw "loadBrainData() was not found after saveSettings()." }
$oneLoad = $shared.Substring($loadStart, $loadEnd - $loadStart)
$oneSave = $shared.Substring($saveStart, $saveEnd - $saveStart)
$shared = $shared.Substring(0, $loadStart) + $oneLoad + "`n`n" + $oneSave + "`n`n" + $shared.Substring($dataStart)

foreach ($signature in @("inline bool settingsLoaded = false;", "inline void loadSettings() {", "inline void saveSettings() {")) {
    if ((Count-Exact $shared $signature) -ne 1) {
        throw "VirgoShared.hpp still contains duplicate or missing declarations: $signature"
    }
}

# The earlier Phase 3.3 patch accidentally removed the PlayLayer guard and
# left a stray '+' in PlayerObject::update(). Restore the whole method safely.
$updateSignature = "    void update(float dt) {"
if ((Count-Exact $main $updateSignature) -ne 1) {
    throw "Expected exactly one PlayerObject::update(float dt) method in main.cpp."
}
$updateStart = $main.IndexOf($updateSignature, [System.StringComparison]::Ordinal)
$updateEnd = Find-FunctionEnd $main $updateStart
$existingUpdate = $main.Substring($updateStart, $updateEnd - $updateStart)
if (-not $existingUpdate.Contains("s_sim.onPlayerUpdate(this);") -or
    -not $existingUpdate.Contains("PlayerObject::update(dt);")) {
    throw "PlayerObject::update() did not match the known VIRGO method; refusing to overwrite it."
}
$fixedUpdate = @'
    void update(float dt) {
        if (!s_sim.isSimulating()) {
            s_sim.onPlayerUpdate(this);
        }
        PlayerObject::update(dt);
        if (PlayLayer::get() && !s_sim.isSimulating()) {
            s_sim.setFrameDelta(dt);
            virgo::brainRecordProgress(getPositionX());
        }
    }
'@
$fixedUpdate = $fixedUpdate.TrimEnd()
$main = $main.Substring(0, $updateStart) + $fixedUpdate + $main.Substring($updateEnd)

# Confirm Phase 3.3's data model, hooks, and controls are present before build.
$requiredMain = @(
    '#include "VirgoBrain.hpp"',
    'virgo::brainBeginLevel(',
    'virgo::brainRecordLevelDeath(',
    'virgo::brainRecordEffect(',
    'virgo::brainRecordOrb(',
    'virgo::brainRecordProgress(getPositionX());'
)
foreach ($item in $requiredMain) {
    if (-not $main.Contains($item)) { throw "Phase 3.3 main.cpp integration is missing: $item" }
}
$requiredUI = @(
    '#include "VirgoBrain.hpp"',
    'AUTOPLAY PRESETS',
    'ZOINK',
    'TRICK',
    'DOGGIE',
    'NETTERMIND',
    'virgo::autoPlayMode = virgo::autoPlayMode == 0 ? 1 : 0;'
)
foreach ($item in $requiredUI) {
    if (-not $ui.Contains($item)) { throw "Phase 3.3 GUI integration is missing: $item" }
}
if (-not $brain.Contains('inline void brainBeginLevel(') -or
    -not $brain.Contains('inline void brainRecordOrb(')) {
    throw "VirgoBrain.hpp is missing the level/orb learning foundation."
}

# Write only after the cleanup and all Phase 3.3 validation checks succeed.
Write-TextLF "src/VirgoShared.hpp" $shared
Write-TextLF "src/main.cpp" $main
Write-Host "VIRGO Phase 3.3 repair complete: shared settings deduplicated and player update restored."
