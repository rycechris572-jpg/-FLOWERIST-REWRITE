// Flowerist Rewrite - Cosmic Dashboard UI
// Built with Cocos2d/Geode primitives; no external image assets required.
#include <Geode/Geode.hpp>
#include <Geode/modify/CCDirector.hpp>
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
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
    auto node = CCLabelBMFont::create(value.c_str(), font);
    if (!node) return nullptr;
    const auto size = node->getContentSize();
    node->setScale(std::min({.78f, maxW / std::max(1.f, size.width), maxH / std::max(1.f, size.height)}));
    node->setColor(tint);
    node->setPosition({x, y});
    parent->addChild(node, 20);
    return node;
}

void card(CCNode* parent, float x, float y, float w, float h,
          ccColor4B fill = rgba(20, 25, 49, 250)) {
    // Offset shadow and bevelled inset create a layered, 3D-style surface.
    rect(parent, x + 3, y - 4, w, h, rgba(3, 5, 17, 220), 1);
    rect(parent, x, y, w, h, rgba(94, 82, 148), 2);
    rect(parent, x + 1, y + 1, w - 2, h - 2, fill, 3);
    rect(parent, x + 5, y + h - 5, w - 10, 1, rgba(150, 139, 205, 125), 4);
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
        rect(parent, x - w * .5f + 5, y + h * .5f - 5, w - 10, 1, rgba(205, 207, 255, 90), 10);
        text(parent, caption, x, y, w - 12, h * .58f, ccc3(247, 242, 255));
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

        text(m_panel, "FLOWERIST", 91, PANEL_H - 27, 144, 24, ccc3(244, 204, 255), "goldFont.fnt");
        text(m_panel, "COSMIC INTERFACE", 91, PANEL_H - 48, 139, 12, ccc3(132, 168, 231));
        text(m_panel, "BUILD 02  /  60 TICK CAP", 91, PANEL_H - 74, 141, 10, ccc3(104, 131, 185));

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
        text(m_content, title, 430, 394, 450, 27, ccc3(238, 220, 255), "goldFont.fnt");
        text(m_content, subtitle, 430, 370, 455, 15, ccc3(145, 170, 220));
        rect(m_content, 196, 354, 470, 1, rgba(97, 107, 159, 170), 4);
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
            card(m_content, 199, 105, 468, 225);
            text(m_content, "SYSTEM TELEMETRY", 226, 299, 210, 19, ccc3(190, 171, 255));
            text(m_content, "INPUT BUFFER", 237, 255, 150, 14, ccc3(142, 167, 226));
            text(m_content, "ACTIVE", 622, 255, 70, 14, ccc3(96, 230, 183));
            text(m_content, "MAXIMUM HORIZON", 237, 217, 170, 14, ccc3(142, 167, 226));
            text(m_content, "60 TICKS", 622, 217, 95, 14, ccc3(236, 218, 255));
            text(m_content, "PERFORMANCE PROFILE", 237, 179, 190, 14, ccc3(142, 167, 226));
            const char* modes[] = {"LOW-END", "BALANCED", "CUSTOM"};
            text(m_content, modes[m_selectedMode], 622, 179, 105, 14, ccc3(236, 218, 255));
            rect(m_content, 222, 157, 423, 1, rgba(98, 111, 164, 150), 5);
            text(m_content, "Console output is optional and opens only when requested.",
                 237, 135, 405, 13, ccc3(136, 153, 198));
            button(m_content, s_consoleVisible ? "CLOSE CONSOLE" : "OPEN CONSOLE", 432, 79, 190, 34, 103, true);
        } else if (m_selected == 1) {
            const char* names[] = {"LOW-END", "BALANCED", "CUSTOM"};
            const char* detail[] = {"Fewer visual effects", "Recommended default", "Manual profile"};
            for (int i = 0; i < 3; ++i) {
                const float x = 278.f + i * 151.f;
                card(m_content, x - 67, 177, 134, 122, i == m_selectedMode ? rgba(39, 34, 74) : rgba(20, 25, 49));
                text(m_content, names[i], x, 264, 118, 17, i == m_selectedMode ? ccc3(226, 198, 255) : ccc3(169, 184, 224));
                text(m_content, detail[i], x, 235, 116, 28, ccc3(132, 154, 203));
                button(m_content, i == m_selectedMode ? "SELECTED" : "SELECT", x, 197, 110, 29,
                       210 + i, true, false, i == m_selectedMode ? 3 : 0);
            }
            text(m_content, "The engine remains capped at 60 ticks in every profile.", 430, 132, 430, 15, ccc3(142, 167, 226));
        } else if (m_selected == 2) {
            text(m_content, "Each stage has its own clean, clickable card.", 430, 333, 430, 14, ccc3(142, 167, 226));
            const char* names[] = {"STAGE 1", "STAGE 2", "STAGE 3", "STAGE 4", "EXTREME", "CUSTOM"};
            const char* detail[] = {"STARTER", "LIGHT", "STANDARD", "ADVANCED", "INTENSE", "PERSONAL"};
            for (int i = 0; i < 6; ++i) {
                const int col = i % 3;
                const int row = i / 3;
                const float x = 278.f + col * 150.f;
                const float y = row == 0 ? 251.f : 151.f;
                card(m_content, x - 65, y - 34, 130, 78,
                     i == m_selectedStage ? rgba(42, 36, 79) : rgba(20, 25, 49));
                text(m_content, names[i], x, y + 10, 118, 16,
                     i == m_selectedStage ? ccc3(232, 205, 255) : ccc3(174, 187, 225));
                text(m_content, detail[i], x, y - 8, 110, 11, ccc3(124, 151, 202));
                button(m_content, i == m_selectedStage ? "ACTIVE" : "CHOOSE", x, y - 27,
                       106, 23, 200 + i, true, false, i == m_selectedStage ? 3 : 0);
            }
        } else if (m_selected == 3) {
            card(m_content, 202, 205, 223, 113);
            card(m_content, 438, 205, 223, 113);
            text(m_content, "INPUT BUFFER", 314, 282, 180, 17, ccc3(211, 195, 255));
            text(m_content, "Engine system", 314, 255, 175, 13, ccc3(137, 160, 210));
            text(m_content, "ACTIVE", 314, 225, 120, 16, ccc3(96, 230, 183));
            text(m_content, "AUTO TUNING", 549, 282, 180, 17, ccc3(211, 195, 255));
            text(m_content, "Future feature", 549, 255, 175, 13, ccc3(137, 160, 210));
            text(m_content, "PLANNED", 549, 225, 120, 16, ccc3(238, 190, 122));
            text(m_content, "Implemented systems are separated from planned tools.", 432, 148, 448, 15, ccc3(142, 167, 226));
        } else if (m_selected == 4) {
            card(m_content, 204, 202, 460, 121);
            text(m_content, "INTERFACE ANIMATIONS", 232, 286, 240, 17, ccc3(211, 195, 255));
            text(m_content, m_animated ? "Smooth transitions enabled" : "Reduced motion enabled", 232, 260, 250, 13, ccc3(137, 160, 210));
            button(m_content, m_animated ? "TURN OFF" : "TURN ON", 585, 269, 112, 31, 102, true);
            text(m_content, "QUIET STATUS MESSAGES", 232, 224, 240, 17, ccc3(211, 195, 255));
            text(m_content, m_quiet ? "Enabled" : "Disabled", 232, 204, 140, 12, ccc3(137, 160, 210));
            button(m_content, m_quiet ? "TURN OFF" : "TURN ON", 585, 211, 112, 31, 101, true);
            text(m_content, "UI animation settings do not change gameplay tick logic.", 432, 155, 445, 15, ccc3(142, 167, 226));
        } else {
            card(m_content, 204, 170, 460, 151);
            text(m_content, "COMPETITIVE PROFILE", 432, 280, 360, 20, ccc3(213, 195, 255), "goldFont.fnt");
            text(m_content, "Check your event rules before enabling assistance.", 432, 245, 410, 15, ccc3(155, 172, 216));
            text(m_content, "This tab is a profile notice, not an anti-cheat bypass.", 432, 215, 410, 15, ccc3(155, 172, 216));
            text(m_content, "MANUAL PROFILE", 432, 185, 200, 14, ccc3(238, 190, 122));
        }
        updateNav();
        if (m_animated) {
            m_content->setOpacity(0);
            m_content->runAction(CCFadeTo::create(.18f, 255));
        }
    }

    void drawLOK() {
        // Created only after the player accepts the guide prompt.
        auto portrait = CCNode::create();
        portrait->setPosition({278, 231});
        m_modalLayer->addChild(portrait, 30);
        auto aura = CCDrawNode::create();
        aura->drawDot({0, 0}, 69, ccc4f(.45f, .30f, .91f, .17f));
        aura->drawDot({0, 0}, 55, ccc4f(.68f, .38f, .98f, .16f));
        portrait->addChild(aura, 0);
        auto face = CCDrawNode::create();
        face->drawDot({0, 0}, 42, ccc4f(1.f, .69f, .84f, 1.f));
        face->drawDot({-29, 4}, 13, ccc4f(1.f, .48f, .72f, .95f));
        face->drawDot({29, 4}, 13, ccc4f(1.f, .48f, .72f, .95f));
        face->drawDot({-13, 7}, 4.5f, ccc4f(.20f, .16f, .34f, 1.f));
        face->drawDot({13, 7}, 4.5f, ccc4f(.20f, .16f, .34f, 1.f));
        face->drawDot({-21, -9}, 5.5f, ccc4f(1.f, .35f, .60f, .42f));
        face->drawDot({21, -9}, 5.5f, ccc4f(1.f, .35f, .60f, .42f));
        face->drawSegment({-6, -12}, {0, -15}, 2, ccc4f(.48f, .19f, .39f, 1.f));
        face->drawSegment({0, -15}, {6, -12}, 2, ccc4f(.48f, .19f, .39f, 1.f));
        face->setScaleX(1.10f);
        portrait->addChild(face, 2);
        flower(portrait, {0, 48}, 18);
        if (m_animated) {
            portrait->runAction(CCRepeatForever::create(CCSequence::create(
                CCEaseSineInOut::create(CCMoveBy::create(.75f, {0, 3})),
                CCEaseSineInOut::create(CCMoveBy::create(.75f, {0, -3})), nullptr)));
        }
        card(m_modalLayer, 377, 168, 258, 125, rgba(25, 28, 58, 255));
        auto tail = CCDrawNode::create();
        tail->drawSegment({377, 215}, {359, 224}, 3, ccc4f(.68f, .61f, .91f, .9f));
        m_modalLayer->addChild(tail, 31);
        text(m_modalLayer, "LOK  /  GUIDE ASSISTANT", 505, 270, 222, 13, ccc3(225, 197, 255));
        static const char* lines[] = {
            "Hi! I'm LOK. Welcome to Flowerist!",
            "Use the left rail to switch between tabs.",
            "Choose a stage from its own card.",
            "The LOG tab opens the console only on request.",
            "You're all set. Enjoy the interface!"
        };
        m_guideSpeech = text(m_modalLayer, lines[m_guideStep], 505, 224, 220, 49, ccc3(227, 229, 250));
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

