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

$brainPath = "src/VirgoBrain.hpp"
$mainPath = "src/main.cpp"

$brain = Read-TextLF $brainPath
$main = Read-TextLF $mainPath

# ------------------------------------------------------------
# Brain: cache the current level's best progress once per level.
# ------------------------------------------------------------

$newCacheDeclarations = @'
inline int brainActiveGameMode = Cube;

// Cached best progress avoids reading Geode saved data on every player update.
inline int brainBestXCache = 0;
inline int brainBestXCacheLevelId = 0;
'@
$brain = Replace-Once $brain `
    'inline int brainActiveGameMode = Cube;' `
    $newCacheDeclarations `
    "Best-progress cache declarations"

$newCacheInitialization = @'
    brainLevelObjectKeys.clear();

    brainBestXCacheLevelId = levelId;
    brainBestXCache = 0;
    if (levelId != 0) {
        if (auto* cacheMod = geode::Mod::get()) {
            brainBestXCache = cacheMod->getSavedValue<int>(
                brainLevelPrefix(levelId) + "best-x", 0
            );
        }
    }
'@
$brain = Replace-Once $brain `
    '    brainLevelObjectKeys.clear();' `
    $newCacheInitialization `
    "Initialize best-progress cache at level start"

$oldProgress = @'
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
'@

$newProgress = @'
inline void brainRecordProgress(float x) {
    if (!brainLearningEnabled || brainActiveLevelId == 0) return;
    auto* mod = geode::Mod::get();
    if (!mod) return;

    // Fallback initialization is performed at most once if this function is
    // reached before the normal level-start cache initialization.
    if (brainBestXCacheLevelId != brainActiveLevelId) {
        const auto cacheKey = brainLevelPrefix(brainActiveLevelId) + "best-x";
        brainBestXCache = mod->getSavedValue<int>(cacheKey, 0);
        brainBestXCacheLevelId = brainActiveLevelId;
    }

    // Compare against memory; persist only when a new 25-unit bucket is reached.
    const int progressBucket = std::max(0, static_cast<int>(x / 25.f)) * 25;
    if (progressBucket > brainBestXCache) {
        brainBestXCache = progressBucket;
        mod->setSavedValue<int>(
            brainLevelPrefix(brainActiveLevelId) + "best-x",
            brainBestXCache
        );
    }
}
'@

$brain = Replace-Once $brain $oldProgress $newProgress "Cache progress instead of reading saved data per frame"

# ------------------------------------------------------------
# Brain: keep object indexing in memory; persist one count after scan.
# ------------------------------------------------------------

$oldObjectRecorder = @'
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
'@

$newObjectRecorder = @'
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

    // Keep the complete in-memory map for Autoplay's hazard lookahead.
    // Do not perform Geode saved-data reads/writes inside this per-object loop.
    brainLevelObjects.push_back({objectId, objectType, x, y, qx, qy});
}

inline void brainCommitLevelLayout() {
    if (!brainLearningEnabled || brainActiveLevelId == 0) return;
    if (auto* mod = geode::Mod::get()) {
        // One persisted write after indexing, rather than multiple operations
        // for every unique object in a large online level.
        mod->setSavedValue<std::uint64_t>(
            brainLevelPrefix(brainActiveLevelId) + "layout-objects",
            static_cast<std::uint64_t>(brainLevelObjects.size())
        );
    }
}
'@

$brain = Replace-Once $brain $oldObjectRecorder $newObjectRecorder "Remove per-object persistent operations"

$oldBestX = @'
inline int brainCurrentLevelBestX() {
    if (brainActiveLevelId == 0) return 0;
    auto* mod = geode::Mod::get();
    return mod ? mod->getSavedValue<int>(
        brainLevelPrefix(brainActiveLevelId) + "best-x", 0) : 0;
}
'@

$newBestX = @'
inline int brainCurrentLevelBestX() {
    if (brainActiveLevelId == 0) return 0;
    return brainBestXCacheLevelId == brainActiveLevelId ? brainBestXCache : 0;
}
'@

$brain = Replace-Once $brain $oldBestX $newBestX "Read best progress from cache"

# ------------------------------------------------------------
# Main: commit the aggregate layout count once after indexing.
# ------------------------------------------------------------

$oldScanTail = @'
        if (m_objects) {
            for (auto* object : CCArrayExt<GameObject>(m_objects)) {
                virgo::brainRecordLevelObject(object);
            }
        }

        log::info(
'@

$newScanTail = @'
        if (m_objects) {
            for (auto* object : CCArrayExt<GameObject>(m_objects)) {
                virgo::brainRecordLevelObject(object);
            }
        }

        virgo::brainCommitLevelLayout();

        log::info(
'@

$main = Replace-Once $main $oldScanTail $newScanTail "Commit layout statistics after object indexing"

# All replacements are validated before either file is written.
Write-TextLF $brainPath $brain
Write-TextLF $mainPath $main

Write-Host "VIRGO Brain loading fix applied."
Write-Host "Object map remains in memory for Autoplay."
Write-Host "Layout statistics are saved once after indexing."
Write-Host "Best progress is cached and saved only when it increases."
