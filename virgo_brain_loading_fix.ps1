$ErrorActionPreference = 'Stop'

$repoRoot = (Get-Location).Path
$brainPath = Join-Path $repoRoot 'src/VirgoBrain.hpp'
if (-not (Test-Path -LiteralPath $brainPath -PathType Leaf)) {
    throw "Could not find src/VirgoBrain.hpp. Run this script from the VIRGO repository root."
}

$encoding = New-Object System.Text.UTF8Encoding($false)
$source = [System.IO.File]::ReadAllText($brainPath)

# Target only the level-object recorder. Its in-memory indexing stays intact;
# the per-object Geode saved-data reads/writes are removed from level loading.
$pattern = '(?s)(inline void brainRecordLevelObject\(GameObject\* object\)\s*\{.*?brainLevelObjects\.push_back\(\{objectId, objectType, x, y, qx, qy\}\);\s*)(if \(!brainLearningEnabled\s*\|\|\s*brainActiveLevelId\s*==\s*0\).*?)(\r?\n\})'
$matches = [regex]::Matches($source, $pattern)

if ($matches.Count -eq 0) {
    # Idempotent success if the function has already been made in-memory-only.
    $functionMatch = [regex]::Match(
        $source,
        '(?s)inline void brainRecordLevelObject\(GameObject\* object\)\s*\{.*?\r?\n\}\s*\r?\n\s*inline std::uint64_t brainCurrentLevelLayoutCount'
    )
    if ($functionMatch.Success -and
        $functionMatch.Value.Contains('brainLevelObjects.push_back') -and
        -not $functionMatch.Value.Contains('getSavedValue') -and
        -not $functionMatch.Value.Contains('setSavedValue')) {
        Write-Host 'VIRGO Brain loading fix is already applied. No files changed.'
        exit 0
    }
    throw 'Could not safely identify the expected brainRecordLevelObject() persistence block. No files were changed.'
}

if ($matches.Count -ne 1) {
    throw "Expected exactly one brainRecordLevelObject() persistence block; found $($matches.Count). No files were changed."
}

$match = $matches[0]
$block = $match.Groups[2].Value
if (-not $block.Contains('getSavedValue') -or
    -not $block.Contains('setSavedValue') -or
    -not $block.Contains('brainIncrement')) {
    throw 'The target block did not contain the expected saved-data operations. No files were changed.'
}

$replacement = $match.Groups[1].Value +
    "`n    // Keep the level map in memory; avoid synchronous per-object saved-data I/O during level loading." +
    $match.Groups[3].Value
$updated = $source.Substring(0, $match.Index) + $replacement +
    $source.Substring($match.Index + $match.Length)

# Verify the target function no longer performs saved-data I/O before writing.
$verify = [regex]::Match(
    $updated,
    '(?s)inline void brainRecordLevelObject\(GameObject\* object\)\s*\{.*?\r?\n\}\s*\r?\n\s*inline std::uint64_t brainCurrentLevelLayoutCount'
)
if (-not $verify.Success -or
    $verify.Value.Contains('getSavedValue') -or
    $verify.Value.Contains('setSavedValue') -or
    -not $verify.Value.Contains('brainLevelObjects.push_back')) {
    throw 'Post-patch validation failed. No files were changed.'
}

$backupPath = "$brainPath.virgo-loading-fix.bak"
if (-not (Test-Path -LiteralPath $backupPath)) {
    Copy-Item -LiteralPath $brainPath -Destination $backupPath
}
[System.IO.File]::WriteAllText($brainPath, $updated, $encoding)
Write-Host 'VIRGO Brain loading patch applied.'
Write-Host 'In-memory level-object indexing and hazard lookup are preserved.'
Write-Host 'Per-object saved-data reads/writes are removed from level loading.'
Write-Host "Backup: $backupPath"
