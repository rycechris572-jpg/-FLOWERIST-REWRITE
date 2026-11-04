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
