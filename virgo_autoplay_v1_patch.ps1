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
