$ErrorActionPreference = 'Stop'

$path = 'src/FloweristUI.cpp'
if (-not (Test-Path -LiteralPath $path)) {
    throw "Could not find $path. Run this script from the repository root."
}

$source = [System.IO.File]::ReadAllText((Resolve-Path -LiteralPath $path))
$source = $source.Replace("`r`n", "`n").Replace("`r", "`n")

function Replace-Once([string]$InputText, [string]$Old, [string]$New, [string]$Description) {
    $count = ([regex]::Matches($InputText, [regex]::Escape($Old))).Count
    if ($count -eq 0) {
        if ($InputText.Contains($New)) { return $InputText }
        throw "Could not find expected source section: $Description"
    }
    if ($count -ne 1) { throw "Expected one match for $Description, found $count. Refusing an ambiguous edit." }
    return $InputText.Replace($Old, $New)
}

function Replace-All([string]$InputText, [string]$Old, [string]$New) {
    return $InputText.Replace($Old, $New)
}

# Give the entire interface the quieter Nova-inspired visual language.
# Keep the existing navigation, controls, callbacks, and data sources intact.
$source = Replace-All $source '"bigFont.fnt"' '"chatFont.fnt"'
$source = Replace-All $source '"goldFont.fnt"' '"chatFont.fnt"'

# Rounded-rectangle controls instead of oversized capsule buttons.
$source = Replace-Once $source 'auto points = roundedPoints(width, height, height * .5f, 16);' 'auto points = roundedPoints(width, height, height * .18f, 16);' 'rounded button geometry'

# Muted plum/lavender button states, with green reserved for positive status.
$buttonPalette = @(
    @('ccc3(20, 35, 64)', 'ccc3(43, 34, 58)'),
    @('ccc3(75, 91, 164)', 'ccc3(91, 67, 119)'),
    @('ccc3(78, 178, 239)', 'ccc3(184, 139, 218)'),
    @('ccc3(127, 153, 255)', 'ccc3(211, 169, 239)'),
    @('ccc3(117, 48, 83)', 'ccc3(101, 53, 91)'),
    @('ccc3(255, 105, 174)', 'ccc3(222, 143, 221)')
)
foreach ($pair in $buttonPalette) { $source = Replace-All $source $pair[0] $pair[1] }

# Re-skin the reusable card component. All tabs use this component, so the
# global card treatment updates the full GUI rather than just the Brain page.
$cardStartMarker = 'void card(CCNode* parent, float x, float y, float w, float h,'
$cardStart = $source.IndexOf($cardStartMarker, [System.StringComparison]::Ordinal)
if ($cardStart -lt 0) { throw 'Could not locate the reusable card() function.' }
$cardOpen = $source.IndexOf('{', $cardStart)
$depth = 0
$cardEnd = -1
for ($i = $cardOpen; $i -lt $source.Length; $i++) {
    if ($source[$i] -eq '{') { $depth++ }
    elseif ($source[$i] -eq '}') {
        $depth--
        if ($depth -eq 0) { $cardEnd = $i + 1; break }
    }
}
if ($cardEnd -lt 0) { throw 'Could not find the end of card() function.' }
$cardReplacement = @'
void card(CCNode* parent, float x, float y, float w, float h,
          ccColor4B fill = rgba(20, 25, 49, 250)) {
    (void)fill;
    // Nova-style surface: quiet plum fill, fine lavender edge, restrained shadow.
    roundedRect(parent, x + 3.f, y - 4.f, w, h, 13.f,
                ccc4f(.006f, .004f, .014f, .72f), 1);
    roundedRect(parent, x, y, w, h, 13.f,
                ccc4f(.37f, .29f, .49f, .92f), 2);
    roundedRect(parent, x + 1.2f, y + 1.2f, w - 2.4f, h - 2.4f, 12.f,
                ccc4f(.055f, .043f, .075f, .99f), 3);
    roundedRect(parent, x + 2.5f, y + 2.5f, w - 5.f, h - 5.f, 10.f,
                ccc4f(.078f, .060f, .101f, .99f), 4);

    auto glint = CCDrawNode::create();
    if (glint) {
        glint->drawSegment({x + 18.f, y + h - 7.f}, {x + w - 18.f, y + h - 7.f},
                           .65f, ccc4f(.83f, .73f, .94f, .16f));
        parent->addChild(glint, 5);
    }
}
'@
$source = $source.Substring(0, $cardStart) + $cardReplacement.TrimEnd() + $source.Substring($cardEnd)

# Rebalance the shared window shell and navigation surfaces to match the new cards.
$themeReplacements = @(
    @('rgba(3, 6, 18, 255)', 'rgba(12, 9, 19, 255)'),
    @('rgba(8, 13, 31, 252)', 'rgba(17, 13, 26, 255)'),
    @('rgba(8, 13, 30, 255)', 'rgba(17, 13, 26, 255)'),
    @('ccc4f(.018f, .028f, .072f, .99f)', 'ccc4f(.035f, .026f, .052f, .99f)'),
    @('ccc4f(.047f, .067f, .14f, .98f)', 'ccc4f(.070f, .052f, .092f, .98f)'),
    @('ccc4f(.38f, .48f, .77f, .97f)', 'ccc4f(.36f, .28f, .49f, .97f)'),
    @('rgba(90, 150, 212, 160)', 'rgba(143, 112, 180, 130)'),
    @('rgba(97, 107, 159, 170)', 'rgba(115, 91, 145, 145)'),
    @('rgba(166, 115, 244, 225)', 'rgba(198, 137, 221, 210)')
)
foreach ($pair in $themeReplacements) { $source = Replace-All $source $pair[0] $pair[1] }

# Replace the Brain tab with a balanced Nova-like dashboard. All values remain
# bound to VIRGO's real saved counters; no grades or autoplay ability are invented.
$brainStartMarker = '} else if (m_selected == 5) {'
$brainEndMarker = "`n`n        updateNav();"
$brainStart = $source.IndexOf($brainStartMarker, [System.StringComparison]::Ordinal)
if ($brainStart -lt 0) { throw 'Could not locate the Brain tab section.' }
if ($source.IndexOf($brainStartMarker, $brainStart + $brainStartMarker.Length, [System.StringComparison]::Ordinal) -ge 0) {
    throw 'Found more than one Brain tab section; refusing an ambiguous edit.'
}
$brainEnd = $source.IndexOf($brainEndMarker, $brainStart, [System.StringComparison]::Ordinal)
if ($brainEnd -lt 0) { throw 'Could not locate the end of the Brain tab section.' }

$brainReplacement = @'
} else if (m_selected == 5) {
            const char* modeNames[virgo::ModeCount] = {
                "CUBE", "WAVE", "SHIP", "BALL", "UFO", "ROBOT", "SPIDER", "SWING"
            };

            // Top dashboard: per-level memory and autoplay settings have room to breathe.
            card(m_content, 205, 210, 220, 130, rgba(27, 29, 65, 255));
            text(m_content, "LEVEL MEMORY", 315, 312, 190, 16,
                 ccc3(220, 204, 255), "chatFont.fnt", .48f);
            const std::string levelLabel = virgo::brainActiveLevelId == 0
                ? "NO LEVEL ACTIVE"
                : "LEVEL ID: " + std::to_string(virgo::brainActiveLevelId);
            text(m_content, levelLabel, 315, 287, 194, 13,
                 ccc3(190, 203, 241), "chatFont.fnt", .40f);
            const std::string sessionLine = "SESSIONS " +
                std::to_string(virgo::brainCurrentLevelSessions()) + "   DEATHS " +
                std::to_string(virgo::brainCurrentLevelDeaths());
            text(m_content, sessionLine, 315, 263, 196, 12,
                 ccc3(174, 163, 192), "chatFont.fnt", .36f);
            const std::string objectLine = "ORBS " +
                std::to_string(virgo::brainCurrentLevelOrbs()) + "   EFFECTS " +
                std::to_string(virgo::brainCurrentLevelEffects()) + "   BEST X " +
                std::to_string(virgo::brainCurrentLevelBestX());
            text(m_content, objectLine, 315, 240, 198, 12,
                 ccc3(174, 163, 192), "chatFont.fnt", .33f);
            text(m_content, "TRACKED LEVELS: " + std::to_string(virgo::brainTrackedLevels()),
                 315, 219, 194, 11, ccc3(146, 134, 170), "chatFont.fnt", .34f);

            card(m_content, 438, 210, 220, 130, rgba(35, 25, 64, 255));
            text(m_content, "AUTOPLAY CONFIG", 548, 312, 190, 16,
                 ccc3(220, 204, 255), "chatFont.fnt", .48f);
            button(m_content, virgo::brainLearningEnabled ? "BRAIN ON" : "BRAIN OFF",
                   493, 276, 88, 24, 400, true, false, virgo::brainLearningEnabled ? 3 : 2);
            button(m_content, virgo::autoPlayMode == 0 ? "NORMAL" : "PRACTICE",
                   602, 276, 96, 24, 401, true, false, 1);
            text(m_content, "EXECUTION: NOT ACTIVE YET", 548, 245, 194, 12,
                 ccc3(213, 171, 132), "chatFont.fnt", .34f);
            text(m_content, "SETUP IS SAVED", 548, 225, 194, 11,
                 ccc3(157, 143, 183), "chatFont.fnt", .33f);

            // Presets are a separate, evenly-spaced control row.
            card(m_content, 205, 137, 453, 62, rgba(24, 27, 58, 255));
            text(m_content, "AUTOPLAY PRESETS", 430, 177, 250, 14,
                 ccc3(205, 183, 222), "chatFont.fnt", .43f);
            const char* presetNames[] = {"ZOINK", "TRICK", "DOGGIE", "NETTERMIND"};
            const float presetX[] = {260.f, 370.f, 480.f, 590.f};
            for (int i = 0; i < 4; ++i) {
                button(m_content, presetNames[i], presetX[i], 153, 96, 23,
                       410 + i, true, false, virgo::autoPlayPreset == i ? 3 : 0);
            }

            // Gamemode activity is arranged in two columns to keep every row legible.
            card(m_content, 205, 8, 453, 118, rgba(23, 26, 55, 255));
            text(m_content, "GAMEMODE ACTIVITY", 430, 110, 240, 14,
                 ccc3(220, 204, 255), "chatFont.fnt", .46f);
            text(m_content, "MODE", 257, 91, 65, 10,
                 ccc3(143, 132, 166), "chatFont.fnt", .32f);
            text(m_content, "PRED / SAFE / DEATHS", 355, 91, 145, 10,
                 ccc3(143, 132, 166), "chatFont.fnt", .29f);
            text(m_content, "MODE", 483, 91, 65, 10,
                 ccc3(143, 132, 166), "chatFont.fnt", .32f);
            text(m_content, "PRED / SAFE / DEATHS", 580, 91, 145, 10,
                 ccc3(143, 132, 166), "chatFont.fnt", .29f);
            rect(m_content, 222, 85, 420, 1, rgba(115, 91, 145, 115), 7);

            for (int row = 0; row < 4; ++row) {
                const int leftMode = row;
                const int rightMode = row + 4;
                const auto leftIndex = static_cast<std::size_t>(leftMode);
                const auto rightIndex = static_cast<std::size_t>(rightMode);
                const float rowY = 76.f - static_cast<float>(row) * 16.f;
                const ccColor3B rowTint = (row % 2 == 0)
                    ? ccc3(225, 218, 237) : ccc3(174, 163, 192);

                text(m_content, modeNames[leftMode], 257, rowY, 65, 11,
                     rowTint, "chatFont.fnt", .36f);
                const std::string leftStats = "P " + std::to_string(virgo::predictionsByMode[leftIndex]) +
                    "  /  " + std::to_string(virgo::safeDelaysByMode[leftIndex]) +
                    "  /  " + std::to_string(virgo::deathsByMode[leftIndex]);
                text(m_content, leftStats, 355, rowY, 145, 11,
                     ccc3(209, 198, 224), "chatFont.fnt", .31f);

                text(m_content, modeNames[rightMode], 483, rowY, 65, 11,
                     rowTint, "chatFont.fnt", .36f);
                const std::string rightStats = "P " + std::to_string(virgo::predictionsByMode[rightIndex]) +
                    "  /  " + std::to_string(virgo::safeDelaysByMode[rightIndex]) +
                    "  /  " + std::to_string(virgo::deathsByMode[rightIndex]);
                text(m_content, rightStats, 580, rowY, 145, 11,
                     ccc3(209, 198, 224), "chatFont.fnt", .31f);
            }
        }
'@

$source = $source.Substring(0, $brainStart) + $brainReplacement.TrimEnd() + $source.Substring($brainEnd)

# Sanity checks before writing: expected controls and all six tabs are retained.
foreach ($required in @(
    'const char* TAB_NAMES[TAB_COUNT]',
    'void buildContent()',
    'AUTOPLAY CONFIG',
    'GAMEMODE ACTIVITY',
    'virgo::brainCurrentLevelSessions()',
    'virgo::predictionsByMode[rightIndex]',
    'void drawLOK()'
)) {
    if (-not $source.Contains($required)) { throw "Post-patch validation failed; missing: $required" }
}

[System.IO.File]::WriteAllText((Resolve-Path -LiteralPath $path), $source, [System.Text.UTF8Encoding]::new($false))
Write-Host 'Nova-style VIRGO UI patch applied to src/FloweristUI.cpp.'
