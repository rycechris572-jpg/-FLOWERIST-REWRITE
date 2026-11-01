// Flowerist Rewrite - Cosmic Dashboard UI
// Built with Cocos2d/Geode primitives; no external image assets required.
#include <Geode/Geode.hpp>
#include <Geode/modify/CCDirector.hpp>
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <sstream>
#include <vector>

using namespace geode::prelude;

namespace {
constexpr float PANEL_W = 704.f;
constexpr float PANEL_H = 448.f;
constexpr float NAV_W = 174.f;
constexpr float PI = 3.1415926535f;
constexpr int TAB_COUNT = 6;
bool s_consoleVisible = false;
const char* TAB_NAMES[TAB_COUNT] = {"OVERVIEW", "MODES", "STAGES", "ASSISTS", "SETTINGS", "COMPETITIVE"};
const char* TAB_INFO[TAB_COUNT] = {
    "System status and recent activity",
    "Choose how Flowerist handles its workload",
    "Select a stage profile",
    "See the active assistance systems",
    "Tune the interface and performance",
    "A profile for rule-conscious play"
};

ccColor4B rgba(int r, int g, int b, int a = 255) { return ccc4(r, g, b, a); }

CCLayerColor* rect(CCNode* parent, float x, float y, float w, float h, ccColor4B c, int z = 0) {
    auto node = CCLayerColor::create(c, w, h);
    if (!node) return nullptr;
    node->setPosition({x, y});
    parent->addChild(node, z);
    return node;
}

CCLabelBMFont* text(CCNode* parent, const std::string& value, float x, float y,
                    float maxW, float maxH, ccColor3B tint = ccc3(225, 226, 246),
                    const char* font = "bigFont.fnt") {
    // Long descriptions wrap into readable lines. Short labels (including every
    // navigation/tab label) remain single-line and keep their existing styling.
    std::string layout = value;
    if (maxH >= 22.f && value.find('\n') == std::string::npos) {
        std::istringstream words(value);
        std::string word, line, wrapped;
        const float lineLimit = maxW / .78f;
        while (words >> word) {
            const std::string candidate = line.empty() ? word : line + " " + word;
            auto probe = CCLabelBMFont::create(candidate.c_str(), font);
            const float width = probe ? probe->getContentSize().width : lineLimit + 1.f;
            if (!line.empty() && width > lineLimit) {
                if (!wrapped.empty()) wrapped += "\n";
                wrapped += line;
                line = word;
            } else {
                line = candidate;
            }
        }
        if (!line.empty()) {
            if (!wrapped.empty()) wrapped += "\n";
            wrapped += line;
        }
        if (!wrapped.empty()) layout = wrapped;
    }

    auto node = CCLabelBMFont::create(layout.c_str(), font);
    if (!node) return nullptr;
    const auto size = node->getContentSize();
    const float fitX = maxW / std::max(1.f, size.width);
    const float fitY = maxH / std::max(1.f, size.height);
    node->setScale(std::min({.78f, fitX, fitY}));
    node->setColor(tint);
    node->setAnchorPoint({.5f, .5f});
    node->setPosition({x, y});
    parent->addChild(node, 20);
    return node;
}

void card(CCNode* parent, float x, float y, float w, float h,
          ccColor4B fill = rgba(20, 25, 49, 250)) {
    // Layered panels: shadow, violet rim, deep inset and fine top/side highlights.
    rect(parent, x + 4, y - 5, w, h, rgba(2, 4, 15, 225), 1);
    rect(parent, x, y, w, h, rgba(91, 83, 151), 2);
    rect(parent, x + 1, y + 1, w - 2, h - 2, rgba(12, 16, 37), 3);
    rect(parent, x + 3, y + 3, w - 6, h - 6, fill, 4);
    rect(parent, x + 10, y + h - 5, w - 20, 1, rgba(171, 154, 234, 145), 5);
    rect(parent, x + 5, y + 9, 2, h - 18, rgba(133, 111, 220, 120), 5);
}

void star(CCNode* parent, float x, float y, float size, unsigned char opacity, float delay) {
    auto dot = CCLayerColor::create(rgba(202, 220, 255, opacity), size * 2, size * 2);
    if (!dot) return;
    dot->setPosition({x, y});
    parent->addChild(dot, -2);
    dot->runAction(CCSequence::create(
        CCDelayTime::create(delay),
        CCRepeatForever::create(CCSequence::create(
            CCFadeTo::create(1.1f, 55), CCFadeTo::create(1.1f, opacity), nullptr)), nullptr));
}

CCDrawNode* orbitalRing(CCNode* parent, CCPoint center, float rx, float ry, int z = 3) {
    auto node = CCDrawNode::create();
    if (!node) return nullptr;
    for (int i = 0; i < 64; ++i) {
        float a = i * 2.f * PI / 64.f;
        float b = (i + 1) * 2.f * PI / 64.f;
        node->drawSegment({center.x + std::cos(a) * rx, center.y + std::sin(a) * ry},
                          {center.x + std::cos(b) * rx, center.y + std::sin(b) * ry},
                          1.f, ccc4f(.53f, .70f, 1.f, .42f));
    }
    parent->addChild(node, z);
    return node;
}

void flower(CCNode* parent, CCPoint at, float radius) {
    auto node = CCDrawNode::create();
    if (!node) return;
    node->setPosition(at);
    for (int i = 0; i < 6; ++i) {
        const float a = i * PI / 3.f - PI / 2.f;
        node->drawDot({std::cos(a) * radius * .62f, std::sin(a) * radius * .62f},
                      radius * .39f, ccc4f(1.f, .43f, .76f, .98f));
    }
    node->drawDot({0, 0}, radius * .34f, ccc4f(1.f, .84f, .48f, 1.f));
    parent->addChild(node, 4);
}
}

class FloweristOverlay final : public CCLayer {
    struct Hit {
        int tag;
        CCPoint center;
        float width;
        float height;
        CCNode* space;
        CCLayerColor* plate;
        CCLayerColor* glow;
        ccColor3B normal;
        ccColor3B hot;
        bool page;
        bool modal;
    };

    CCNode* m_panel = nullptr;
    CCNode* m_content = nullptr;
    CCNode* m_modalLayer = nullptr;
    std::vector<Hit> m_hits;
    int m_selected = 0;
    int m_selectedStage = 0;
    int m_selectedMode = 1;
    int m_modal = 0; // 0 = hidden, 1 = yes/no prompt, 2 = LOK guide
    int m_guideStep = 0;
    int m_hovered = -1;
    float m_elapsed = 0.f;
    bool m_closing = false;
    bool m_quiet = false;
    bool m_animated = true;
    CCLabelBMFont* m_guideSpeech = nullptr;
    static FloweristOverlay* s_active;

    void clearPageHits() {
        m_hits.erase(std::remove_if(m_hits.begin(), m_hits.end(), [](const Hit& h) { return h.page; }), m_hits.end());
        m_hovered = -1;
    }
    void clearModalHits() {
        m_hits.erase(std::remove_if(m_hits.begin(), m_hits.end(), [](const Hit& h) { return h.modal; }), m_hits.end());
        m_hovered = -1;
    }

    void button(CCNode* parent, const std::string& caption, float x, float y,
                float w, float h, int tag, bool page = false, bool modal = false, int style = 0) {
        // YES = green, NO = red; other buttons use an indigo/violet palette.
        ccColor3B normal = style == 1 ? ccc3(35, 91, 75) :
                           style == 2 ? ccc3(105, 43, 63) :
                           style == 3 ? ccc3(77, 58, 121) : ccc3(35, 43, 76);
        ccColor3B hot = style == 1 ? ccc3(35, 190, 120) :
                        style == 2 ? ccc3(229, 64, 91) :
                        style == 3 ? ccc3(124, 93, 205) : ccc3(79, 107, 174);
        rect(parent, x - w * .5f + 3, y - h * .5f - 3, w, h, rgba(3, 5, 17, 220), 7);
        auto glow = rect(parent, x - w * .5f - 3, y - h * .5f - 3, w + 6, h + 6, rgba(95, 123, 255, 0), 8);
        auto plate = rect(parent, x - w * .5f, y - h * .5f, w, h,
                          rgba(normal.r, normal.g, normal.b), 9);
        rect(parent, x - w * .5f + 5, y + h * .5f - 5, w - 10, 1, rgba(205, 207, 255, 115), 10);
        rect(parent, x - w * .5f + 4, y - h * .5f + 5, 2, h - 10,
             style == 1 ? rgba(67, 226, 159, 230) :
             style == 2 ? rgba(245, 91, 119, 230) : rgba(147, 126, 240, 210), 10);
        text(parent, caption, x + 2, y, w - 18, h * .58f, ccc3(247, 242, 255));
        m_hits.push_back({tag, {x, y}, w, h, parent, plate, glow, normal, hot, page, modal});
    }

    void updateNav() {
        for (auto& hit : m_hits) {
            if (hit.page || hit.modal || hit.space != m_panel || hit.tag < 0 || hit.tag >= TAB_COUNT) continue;
            const bool selected = hit.tag == m_selected;
            hit.normal = selected ? ccc3(77, 58, 121) : ccc3(35, 43, 76);
            hit.hot = selected ? ccc3(124, 93, 205) : ccc3(79, 107, 174);
            if (hit.plate) hit.plate->setColor(hit.normal);
        }
    }

    void drawBackdrop() {
        auto win = CCDirector::sharedDirector()->getWinSize();
        rect(this, 0, 0, win.width, win.height, rgba(5, 7, 22, 210), -5);
        for (int i = 0; i < 42; ++i) {
            const float x = ((i * 137 + 43) % 997) / 997.f * win.width;
            const float y = ((i * 83 + 19) % 983) / 983.f * win.height;
            star(this, x, y, i % 4 == 0 ? 1.6f : 1.f,
                 static_cast<unsigned char>(90 + (i % 5) * 25), (i % 9) * .11f);
        }
        auto nebula = CCDrawNode::create();
        if (nebula) {
            nebula->drawDot({win.width * .82f, win.height * .78f}, std::min(win.width, win.height) * .19f,
                             ccc4f(.35f, .21f, .66f, .10f));
            nebula->drawDot({win.width * .18f, win.height * .20f}, std::min(win.width, win.height) * .14f,
                             ccc4f(.16f, .52f, .78f, .08f));
            addChild(nebula, -4);
        }
    }

    void decorate() {
        rect(m_panel, 0, 0, PANEL_W, PANEL_H, rgba(77, 65, 130), 0);
        rect(m_panel, 2, 2, PANEL_W - 4, PANEL_H - 4, rgba(14, 17, 39), 1);
        rect(m_panel, 5, 5, PANEL_W - 10, PANEL_H - 10, rgba(22, 22, 49), 2);
        rect(m_panel, 7, 7, NAV_W, PANEL_H - 14, rgba(17, 21, 45), 3);
        rect(m_panel, NAV_W + 7, 8, 1.5f, PANEL_H - 16, rgba(112, 104, 177), 4);

        for (int i = 0; i < 22; ++i) {
            auto dot = CCDrawNode::create();
            if (!dot) continue;
            dot->drawDot({0, 0}, i % 4 == 0 ? 1.5f : .8f, ccc4f(.72f, .79f, 1.f, .40f));
            dot->setPosition({194.f + static_cast<float>((i * 79 + 13) % 487),
                              27.f + static_cast<float>((i * 53 + 9) % 330)});
            m_panel->addChild(dot, 2);
        }
        auto planet = CCDrawNode::create();
        if (planet) {
            planet->drawDot({0, 0}, 23.f, ccc4f(.30f, .38f, .78f, .94f));
            planet->drawDot({-7, 8}, 9.f, ccc4f(.68f, .61f, 1.f, .28f));
            planet->drawSegment({-31, -7}, {31, 8}, 2.f, ccc4f(.65f, .72f, 1.f, .65f));
            planet->setPosition({PANEL_W - 48, PANEL_H - 43});
            m_panel->addChild(planet, 4);
            auto ring = orbitalRing(m_panel, {PANEL_W - 48, PANEL_H - 43}, 38, 13, 3);
            if (ring) ring->setRotation(-18.f);
        }

        flower(m_panel, {31, PANEL_H - 35}, 10);
        text(m_panel, "FLOWERIST", 99, PANEL_H - 27, 126, 24, ccc3(244, 204, 255), "goldFont.fnt");
        text(m_panel, "COSMIC CONTROL CENTER", 99, PANEL_H - 48, 126, 12, ccc3(132, 168, 231));
        text(m_panel, "BUILD 02  /  60 TICK CAP", 99, PANEL_H - 74, 126, 10, ccc3(104, 131, 185));

        for (int i = 0; i < TAB_COUNT; ++i) {
            const float y = PANEL_H - 118.f - i * 42.f;
            button(m_panel, TAB_NAMES[i], 91, y, 148, 31, i, false, false, i == m_selected ? 3 : 0);
        }
        button(m_panel, "GUI GUIDE", 91, 36, 148, 32, 110);
        text(m_panel, "F2  MENU  /  CLOSE", 91, 12, 140, 9, ccc3(94, 119, 170));
        button(m_panel, "X", PANEL_W - 24, PANEL_H - 25, 30, 28, 100);
        text(m_panel, "FLOWERIST  //  SPACE SYSTEMS", 224, PANEL_H - 28, 380, 13, ccc3(117, 143, 204));
        text(m_panel, "ONLINE", PANEL_W - 95, PANEL_H - 28, 68, 12, ccc3(96, 230, 183));
        updateNav();
    }

    void sectionTitle(const std::string& title, const std::string& subtitle) {
        text(m_content, "FLOWERIST  /  CONTROL CENTER", 224, 414, 300, 10, ccc3(117, 145, 205));
        text(m_content, title, 430, 390, 450, 27, ccc3(238, 220, 255), "goldFont.fnt");
        text(m_content, subtitle, 430, 365, 455, 25, ccc3(145, 170, 220));
        rect(m_content, 196, 346, 470, 1, rgba(97, 107, 159, 170), 4);
        rect(m_content, 196, 346, 72, 2, rgba(166, 115, 244, 225), 5);
    }

    void buildContent() {
        clearPageHits();
        if (m_content) {
            m_content->removeFromParentAndCleanup(true);
            m_content = nullptr;
        }
        m_content = CCNode::create();
        if (!m_content) return;
        m_panel->addChild(m_content, 6);
        sectionTitle(TAB_NAMES[m_selected], TAB_INFO[m_selected]);

        if (m_selected == 0) {
            // Overview: two compact telemetry cards plus a wider action panel.
            card(m_content, 207, 207, 214, 105, rgba(24, 29, 57, 255));
            card(m_content, 438, 207, 214, 105, rgba(28, 25, 59, 255));
            card(m_content, 207, 94, 445, 98, rgba(20, 26, 51, 255));

            text(m_content, "BUFFER ENGINE", 314, 285, 170, 17, ccc3(194, 177, 255));
            text(m_content, "INPUT PREDICTION", 314, 255, 172, 18, ccc3(145, 168, 224));
            text(m_content, "ACTIVE", 314, 228, 140, 17, ccc3(96, 230, 183));
            rect(m_content, 233, 218, 162, 2, rgba(65, 205, 157, 145), 7);

            text(m_content, "LOOKAHEAD CAP", 545, 285, 175, 17, ccc3(194, 177, 255));
            text(m_content, "60 TICKS", 545, 253, 170, 25, ccc3(236, 218, 255), "goldFont.fnt");
            text(m_content, "PERFORMANCE LIMIT", 545, 226, 170, 13, ccc3(135, 157, 207));

            text(m_content, "LIVE CONSOLE", 232, 164, 190, 17, ccc3(208, 195, 255));
            text(m_content, "Console output stays closed until you open it.",
                 232, 135, 255, 32, ccc3(136, 156, 205));
            button(m_content, s_consoleVisible ? "CLOSE CONSOLE" : "OPEN CONSOLE",
                   555, 143, 156, 34, 103, true);
        } else if (m_selected == 1) {
            const char* names[] = {"LOW-END", "BALANCED", "CUSTOM"};
            const char* detail[] = {"Prioritizes a lighter interface", "A balanced default profile", "A manually configured profile"};
            for (int i = 0; i < 3; ++i) {
                const float x = 278.f + i * 151.f;
                card(m_content, x - 69, 169, 138, 148,
                     i == m_selectedMode ? rgba(42, 34, 78, 255) : rgba(20, 26, 51, 255));
                text(m_content, names[i], x, 279, 124, 18,
                     i == m_selectedMode ? ccc3(232, 211, 255) : ccc3(177, 188, 226));
                text(m_content, detail[i], x, 239, 118, 38, ccc3(138, 158, 208));
                button(m_content, i == m_selectedMode ? "SELECTED" : "SELECT",
                       x, 192, 112, 31, 210 + i, true, false, i == m_selectedMode ? 3 : 0);
            }
            text(m_content, "The engine remains capped at 60 ticks in every profile.",
                 430, 125, 430, 27, ccc3(142, 167, 226));
        } else if (m_selected == 2) {
            text(m_content, "Choose a stage profile. Each option has its own settings card.",
                 430, 326, 430, 26, ccc3(142, 167, 226));
            const char* names[] = {"STAGE 1", "STAGE 2", "STAGE 3", "STAGE 4", "EXTREME", "CUSTOM"};
            const char* detail[] = {"STARTER", "LIGHT", "STANDARD", "ADVANCED", "INTENSE", "PERSONAL"};
            const ccColor4B fills[] = {
                rgba(24, 42, 69), rgba(25, 39, 72), rgba(36, 32, 76),
                rgba(43, 31, 72), rgba(61, 28, 68), rgba(29, 39, 62)
            };
            for (int i = 0; i < 6; ++i) {
                const int col = i % 3;
                const int row = i / 3;
                const float x = 278.f + col * 150.f;
                const float y = row == 0 ? 244.f : 137.f;
                card(m_content, x - 67, y - 38, 134, 86,
                     i == m_selectedStage ? rgba(51, 39, 88, 255) : fills[i]);
                text(m_content, names[i], x, y + 18, 118, 17,
                     i == m_selectedStage ? ccc3(238, 214, 255) : ccc3(184, 194, 233));
                text(m_content, detail[i], x, y - 3, 114, 13, ccc3(135, 161, 218));
                button(m_content, i == m_selectedStage ? "ACTIVE" : "CHOOSE", x, y - 26,
                       108, 24, 200 + i, true, false, i == m_selectedStage ? 3 : 0);
            }
        } else if (m_selected == 3) {
            card(m_content, 202, 194, 223, 132, rgba(23, 31, 57, 255));
            card(m_content, 438, 194, 223, 132, rgba(34, 27, 62, 255));
            text(m_content, "INPUT BUFFER", 314, 286, 180, 18, ccc3(216, 199, 255));
            text(m_content, "Core prediction system", 314, 256, 178, 27, ccc3(145, 166, 215));
            text(m_content, "ACTIVE", 314, 222, 120, 16, ccc3(96, 230, 183));
            text(m_content, "LEARN FROM ATTEMPTS", 549, 286, 192, 18, ccc3(216, 199, 255));
            text(m_content, "Planned for the next phase", 549, 256, 178, 27, ccc3(155, 165, 211));
            text(m_content, "PLANNED", 549, 222, 120, 16, ccc3(238, 190, 122));
            text(m_content, "Tuning and learning features will be developed in Phase 3.",
                 432, 139, 448, 30, ccc3(142, 167, 226));
        } else if (m_selected == 4) {
            card(m_content, 204, 177, 460, 157, rgba(22, 27, 54, 255));
            text(m_content, "INTERFACE ANIMATIONS", 235, 294, 245, 18, ccc3(216, 199, 255));
            text(m_content, m_animated ? "Smooth transitions are enabled" : "Reduced motion is enabled",
                 235, 266, 255, 26, ccc3(142, 164, 213));
            button(m_content, m_animated ? "TURN OFF" : "TURN ON", 584, 278, 112, 31, 102, true);
            rect(m_content, 226, 243, 416, 1, rgba(95, 108, 159, 130), 6);
            text(m_content, "QUIET STATUS MESSAGES", 235, 220, 245, 18, ccc3(216, 199, 255));
            text(m_content, m_quiet ? "Status messages are hidden" : "Status messages are visible",
                 235, 193, 255, 26, ccc3(142, 164, 213));
            button(m_content, m_quiet ? "TURN OFF" : "TURN ON", 584, 204, 112, 31, 101, true);
        } else {
            card(m_content, 204, 163, 460, 175, rgba(23, 25, 54, 255));
            text(m_content, "COMPETITIVE PROFILE", 432, 295, 360, 23, ccc3(221, 202, 255), "goldFont.fnt");
            text(m_content, "Review the rules of each event before enabling assistance.",
                 432, 253, 410, 30, ccc3(155, 172, 216));
            rect(m_content, 231, 226, 402, 1, rgba(95, 108, 159, 130), 6);
            text(m_content, "This page is a reminder to check event rules, not an anti-cheat bypass.",
                 432, 203, 410, 32, ccc3(155, 172, 216));
            text(m_content, "MANUAL PROFILE", 432, 175, 200, 16, ccc3(238, 190, 122));
        }

        updateNav();
        if (m_animated) {
            // CCNode has no opacity API in this Cocos build; use a subtle scale-in.
            m_content->setScale(.985f);
            m_content->runAction(CCEaseSineOut::create(CCScaleTo::create(.18f, 1.f)));
        }
    }

    void drawLOK() {
        // LOK is an original rounded gamepad/cloud mascot with a flower crest.
        // The Discord-like silhouette is only a visual reference, not a logo copy.
        auto portrait = CCNode::create();
        portrait->setPosition({278, 231});
        m_modalLayer->addChild(portrait, 30);

        auto orbit = CCDrawNode::create();
        orbit->drawSegment({-57, 0}, {57, 0}, 1.4f, ccc4f(.60f, .61f, 1.f, .44f));
        orbit->drawSegment({0, -43}, {0, 43}, 1.1f, ccc4f(.60f, .61f, 1.f, .24f));
        orbit->drawDot({-51, 0}, 3.5f, ccc4f(.63f, .76f, 1.f, .90f));
        orbit->drawDot({51, 0}, 3.5f, ccc4f(1.f, .55f, .81f, .90f));
        portrait->addChild(orbit, 0);
        orbit->runAction(CCRepeatForever::create(CCRotateBy::create(13.f, 360.f)));

        auto aura = CCDrawNode::create();
        aura->drawDot({0, 0}, 69, ccc4f(.45f, .28f, .96f, .18f));
        aura->drawDot({0, 0}, 55, ccc4f(.72f, .39f, .99f, .14f));
        portrait->addChild(aura, 1);
        if (m_animated) {
            aura->runAction(CCRepeatForever::create(CCSequence::create(
                CCEaseSineInOut::create(CCScaleTo::create(1.15f, 1.09f)),
                CCEaseSineInOut::create(CCScaleTo::create(1.15f, .94f)), nullptr)));
        }

        auto shadow = CCDrawNode::create();
        shadow->drawDot({0, -9}, 37, ccc4f(.02f, .03f, .10f, .48f));
        shadow->setScaleX(1.25f);
        portrait->addChild(shadow, 2);

        // Soft rounded gamepad body built from overlapping circles and capsules.
        auto shell = CCDrawNode::create();
        const auto shellColor = ccc4f(.94f, .56f, .82f, 1.f);
        const auto shellShade = ccc4f(.78f, .36f, .72f, 1.f);
        shell->drawSegment({-19, 20}, {19, 20}, 24, shellColor);
        shell->drawSegment({-27, 7}, {27, 7}, 27, shellColor);
        shell->drawSegment({-25, -9}, {25, -9}, 22, shellColor);
        shell->drawDot({-25, 1}, 13.5f, shellColor);
        shell->drawDot({25, 1}, 13.5f, shellColor);
        shell->drawDot({0, 4}, 37.5f, shellColor);
        shell->drawSegment({-19, 25}, {-25, 34}, 7, shellShade);
        shell->drawSegment({19, 25}, {25, 34}, 7, shellShade);
        shell->drawSegment({-16, 29}, {12, 29}, 3, ccc4f(1.f, .90f, .98f, .66f));
        portrait->addChild(shell, 3);

        // Separate eye nodes make blinking smooth and easy to animate.
        auto leftEye = CCDrawNode::create();
        leftEye->drawDot({0, 0}, 4.8f, ccc4f(.18f, .15f, .34f, 1.f));
        leftEye->setPosition({-13, 7});
        portrait->addChild(leftEye, 5);
        auto rightEye = CCDrawNode::create();
        rightEye->drawDot({0, 0}, 4.8f, ccc4f(.18f, .15f, .34f, 1.f));
        rightEye->setPosition({13, 7});
        portrait->addChild(rightEye, 5);

        if (m_animated) {
            auto blink = [] (CCNode* eye, float delay) {
                eye->runAction(CCRepeatForever::create(CCSequence::create(
                    CCDelayTime::create(delay),
                    CCScaleTo::create(.07f, 1.f, .12f),
                    CCScaleTo::create(.10f, 1.f, 1.f),
                    CCDelayTime::create(2.4f), nullptr)));
            };
            blink(leftEye, 1.25f);
            blink(rightEye, 1.25f);
        }

        auto expression = CCDrawNode::create();
        expression->drawDot({-22, -8}, 5.4f, ccc4f(1.f, .31f, .62f, .48f));
        expression->drawDot({22, -8}, 5.4f, ccc4f(1.f, .31f, .62f, .48f));
        expression->drawSegment({-6, -12}, {0, -15}, 2.f, ccc4f(.48f, .18f, .40f, 1.f));
        expression->drawSegment({0, -15}, {6, -12}, 2.f, ccc4f(.48f, .18f, .40f, 1.f));
        portrait->addChild(expression, 5);

        auto crest = CCNode::create();
        crest->setPosition({0, 45});
        portrait->addChild(crest, 6);
        flower(crest, {0, 0}, 17);
        if (m_animated) {
            crest->runAction(CCRepeatForever::create(CCSequence::create(
                CCRotateBy::create(5.5f, 360.f), nullptr)));
            portrait->runAction(CCRepeatForever::create(CCSequence::create(
                CCEaseSineInOut::create(CCMoveBy::create(.82f, {0, 4})),
                CCEaseSineInOut::create(CCMoveBy::create(.82f, {0, -4})), nullptr)));
            shell->runAction(CCRepeatForever::create(CCSequence::create(
                CCEaseSineInOut::create(CCScaleTo::create(1.25f, 1.025f)),
                CCEaseSineInOut::create(CCScaleTo::create(1.25f, 1.f)), nullptr)));
        }

        // Speech bubble has its own entrance and text gets enough height to wrap.
        auto bubble = CCNode::create();
        bubble->setPosition({377, 168});
        m_modalLayer->addChild(bubble, 31);
        card(bubble, -129, -62, 258, 125, rgba(25, 28, 58, 255));
        auto tail = CCDrawNode::create();
        tail->drawSegment({-129, 17}, {-147, 26}, 3, ccc4f(.68f, .61f, .91f, .9f));
        bubble->addChild(tail, 1);

        text(bubble, "LOK  /  GUIDE ASSISTANT", 0, 51, 220, 17, ccc3(225, 197, 255));
        static const char* lines[] = {
            "Hi! I'm LOK. Welcome to Flowerist!",
            "Use the left rail to switch between tabs.",
            "Choose a stage from its own card.",
            "The console opens only when you request it.",
            "You're all set. Enjoy the interface!"
        };
        m_guideSpeech = text(bubble, lines[m_guideStep], 0, 5, 220, 48, ccc3(227, 229, 250));
        if (m_animated) {
            bubble->setScale(.90f);
            bubble->runAction(CCEaseBackOut::create(CCScaleTo::create(.24f, 1.f)));
            if (m_guideSpeech) {
                const float speechScale = m_guideSpeech->getScale();
                m_guideSpeech->setScale(speechScale * .94f);
                m_guideSpeech->runAction(CCEaseSineOut::create(
                    CCScaleTo::create(.18f, speechScale)));
            }
        }
    }

    void closeModal() {
        clearModalHits();
        if (m_modalLayer) {
            m_modalLayer->removeFromParentAndCleanup(true);
            m_modalLayer = nullptr;
        }
        m_guideSpeech = nullptr;
    }

    void renderModal() {
        closeModal();
        if (m_modal == 0) return;
        m_modalLayer = CCNode::create();
        m_panel->addChild(m_modalLayer, 80);
        rect(m_modalLayer, 0, 0, PANEL_W, PANEL_H, rgba(5, 7, 22, 220), 0);
        card(m_modalLayer, 163, 90, 385, 270, rgba(22, 22, 52, 255));
        if (m_modal == 1) {
            text(m_modalLayer, "QUICK HELP", 355, 317, 300, 22, ccc3(226, 198, 255), "goldFont.fnt");
            text(m_modalLayer, "Do you want a guide about the GUI?", 355, 267, 330, 20, ccc3(237, 237, 255));
            text(m_modalLayer, "LOK appears only if you choose YES.", 355, 231, 320, 14, ccc3(151, 170, 218));
            button(m_modalLayer, "YES", 270, 174, 118, 42, 111, false, true, 1);
            button(m_modalLayer, "NO", 440, 174, 118, 42, 112, false, true, 2);
        } else {
            text(m_modalLayer, "FLOWERIST GUI TOUR", 434, 330, 410, 22, ccc3(226, 198, 255), "goldFont.fnt");
            drawLOK();
            button(m_modalLayer, m_guideStep == 4 ? "FINISH" : "NEXT", 495, 123, 116, 31, 113, false, true, 3);
            button(m_modalLayer, "CLOSE", 623, 123, 92, 31, 114, false, true);
            text(m_modalLayer, std::to_string(m_guideStep + 1) + " / 5", 279, 119, 70, 12, ccc3(146, 164, 213));
        }
    }

    void action(int tag) {
        if (tag == 100) { close(); return; }
        if (m_modal == 1) {
            if (tag == 111) { m_modal = 2; m_guideStep = 0; renderModal(); }
            else if (tag == 112) { m_modal = 0; renderModal(); }
            return;
        }
        if (m_modal == 2) {
            if (tag == 113) {
                if (m_guideStep < 4) { ++m_guideStep; renderModal(); }
                else { m_modal = 0; renderModal(); }
            } else if (tag == 114) { m_modal = 0; renderModal(); }
            return;
        }
        if (tag == 110) { m_modal = 1; renderModal(); return; }
        if (tag >= 0 && tag < TAB_COUNT) { m_selected = tag; buildContent(); return; }
        if (tag == 103) {
            if (!s_consoleVisible) {
                if (AllocConsole()) {
                    FILE* file = nullptr;
                    freopen_s(&file, "CONOUT$", "w", stdout);
                    freopen_s(&file, "CONOUT$", "w", stderr);
                    SetConsoleTitleA("Flowerist Rewrite - Console");
                    s_consoleVisible = true;
                    printf("[Flowerist] Console opened from the LOG tab.\n");
                } else s_consoleVisible = GetConsoleWindow() != nullptr;
            } else { FreeConsole(); s_consoleVisible = false; }
        } else if (tag >= 200 && tag < 206) {
            m_selectedStage = tag - 200;
        } else if (tag >= 210 && tag < 213) {
            m_selectedMode = tag - 210;
        } else if (tag == 102) {
            m_animated = !m_animated;
        } else if (tag == 101) {
            m_quiet = !m_quiet;
        }
        buildContent();
    }

    bool allowed(const Hit& hit) const { return m_modal == 0 ? !hit.modal : hit.modal; }

public:
    static FloweristOverlay* active() { return s_active; }
    static FloweristOverlay* create() {
        auto layer = new FloweristOverlay();
        if (layer && layer->init()) { layer->autorelease(); return layer; }
        delete layer;
        return nullptr;
    }

    bool init() override {
        if (!CCLayer::init()) return false;
        // Capture first, then route clicks to our own controls. No CCMenu items
        // are used, so the catch layer can reliably swallow clicks over the UI.
        setTouchEnabled(true);
        setTouchMode(kCCTouchesOneByOne);
        setTouchPriority(-129);
        drawBackdrop();
        const auto win = CCDirector::sharedDirector()->getWinSize();
        m_panel = CCNode::create();
        m_panel->setContentSize({PANEL_W, PANEL_H});
        m_panel->setAnchorPoint({.5f, .5f});
        m_panel->ignoreAnchorPointForPosition(false);
        m_panel->setPosition({win.width * .5f, win.height * .5f});
        const float fit = std::min({1.f, (win.width - 24.f) / PANEL_W, (win.height - 24.f) / PANEL_H});
        m_panel->setScale(fit * .90f);
        addChild(m_panel, 10);
        decorate();
        buildContent();
        if (m_animated) m_panel->runAction(CCEaseBackOut::create(CCScaleTo::create(.34f, fit)));
        else m_panel->setScale(fit);
        s_active = this;
        scheduleUpdate();
        return true;
    }

    bool ccTouchBegan(CCTouch*, CCEvent*) override { return !m_closing; }

    void ccTouchEnded(CCTouch* touch, CCEvent*) override {
        if (!touch || m_closing || !m_panel) return;
        const CCPoint panelPoint = m_panel->convertToNodeSpace(touch->getLocation());
        const CCPoint worldPoint = m_panel->convertToWorldSpace(panelPoint);
        for (auto it = m_hits.rbegin(); it != m_hits.rend(); ++it) {
            if (!allowed(*it) || !it->space) continue;
            const CCPoint p = it->space->convertToNodeSpace(worldPoint);
            if (std::fabs(p.x - it->center.x) <= it->width * .5f &&
                std::fabs(p.y - it->center.y) <= it->height * .5f) {
                action(it->tag);
                return;
            }
        }
        // Empty overlay space is intentionally consumed and does nothing.
    }

    void update(float dt) override {
        if (m_closing) return;
        m_elapsed += dt;
        if (m_elapsed < .045f) return;
        m_elapsed = 0.f;
        HWND window = GetForegroundWindow();
        if (!window) return;
        DWORD process = 0;
        GetWindowThreadProcessId(window, &process);
        if (process != GetCurrentProcessId()) return;
        POINT cursor{};
        if (!GetCursorPos(&cursor) || !ScreenToClient(window, &cursor)) return;
        RECT rc{};
        GetClientRect(window, &rc);
        const float cw = static_cast<float>(rc.right - rc.left);
        const float ch = static_cast<float>(rc.bottom - rc.top);
        if (cw <= 0 || ch <= 0) return;
        const auto win = CCDirector::sharedDirector()->getWinSize();
        const CCPoint gl{cursor.x * win.width / cw, (ch - cursor.y) * win.height / ch};
        const CCPoint panelPoint = m_panel->convertToNodeSpace(gl);
        const CCPoint worldPoint = m_panel->convertToWorldSpace(panelPoint);
        int hover = -1;
        for (size_t i = 0; i < m_hits.size(); ++i) {
            auto& hit = m_hits[i];
            if (!allowed(hit) || !hit.space) continue;
            const CCPoint p = hit.space->convertToNodeSpace(worldPoint);
            if (std::fabs(p.x - hit.center.x) <= hit.width * .5f &&
                std::fabs(p.y - hit.center.y) <= hit.height * .5f) { hover = static_cast<int>(i); break; }
        }
        if (hover != m_hovered) {
            m_hovered = hover;
            for (size_t i = 0; i < m_hits.size(); ++i) {
                auto& hit = m_hits[i];
                if (!allowed(hit)) continue;
                const bool hot = static_cast<int>(i) == hover;
                if (hit.plate) hit.plate->setColor(hot ? hit.hot : hit.normal);
                if (hit.glow) {
                    hit.glow->stopAllActions();
                    hit.glow->runAction(CCFadeTo::create(m_animated ? .12f : 0.f, hot ? 175 : 0));
                }
            }
        }
    }

    void onExit() override {
        if (s_active == this) s_active = nullptr;
        CCLayer::onExit();
    }

    void close() {
        if (m_closing) return;
        m_closing = true;
        unscheduleUpdate();
        setTouchEnabled(false);
        if (m_panel && m_animated) {
            m_panel->runAction(CCEaseSineIn::create(CCScaleTo::create(.14f, m_panel->getScale() * .88f)));
            runAction(CCSequence::create(CCDelayTime::create(.15f),
                CCCallFunc::create(this, callfunc_selector(FloweristOverlay::removeSelf)), nullptr));
        } else removeSelf();
    }
    void removeSelf() { removeFromParentAndCleanup(true); }
};

FloweristOverlay* FloweristOverlay::s_active = nullptr;

class $modify(FloweristGlobalFrame, CCDirector) {
    void drawScene() {
        static bool lastDown = false;
        const bool down = (GetAsyncKeyState(VK_F2) & 0x8000) != 0;
        if (down && !lastDown) {
            if (auto overlay = FloweristOverlay::active()) overlay->close();
            else if (auto scene = CCDirector::sharedDirector()->getRunningScene()) {
                if (auto overlay = FloweristOverlay::create()) scene->addChild(overlay, 999999);
            }
        }
        lastDown = down;
        CCDirector::drawScene();
    }
};

