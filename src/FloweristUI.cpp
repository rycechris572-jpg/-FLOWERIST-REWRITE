// VIRGO - Premium Cosmic Dashboard UI
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
    "Choose how VIRGO handles its workload",
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

void roundedRect(CCNode* parent, float x, float y, float w, float h, float radius,
                 ccColor4F color, int z = 0) {
    auto node = CCDrawNode::create();
    if (!node) return;
    node->setPosition({x + w * .5f, y + h * .5f});
    const float r = std::min(radius, std::min(w, h) * .5f);
    const float left = -w * .5f + r;
    const float right = w * .5f - r;
    const float bottom = -h * .5f + r;
    const float top = h * .5f - r;
    node->drawSegment({left, top}, {right, top}, r, color);
    node->drawSegment({left, bottom}, {right, bottom}, r, color);
    node->drawSegment({left, bottom}, {left, top}, r, color);
    node->drawSegment({right, bottom}, {right, top}, r, color);
    node->drawDot({left, top}, r, color);
    node->drawDot({right, top}, r, color);
    node->drawDot({left, bottom}, r, color);
    node->drawDot({right, bottom}, r, color);
    parent->addChild(node, z);
}

CCLabelBMFont* text(CCNode* parent, const std::string& value, float x, float y,
                    float maxW, float maxH, ccColor3B tint = ccc3(225, 226, 246),
                    const char* font = "bigFont.fnt", float maxScale = .40f) {
    // Long descriptions wrap into readable lines. Short labels (including every
    // navigation/tab label) remain single-line and keep their existing styling.
    std::string layout = value;
    if (maxH >= 22.f && value.find('\n') == std::string::npos) {
        std::istringstream words(value);
        std::string word, line, wrapped;
        const float lineLimit = maxW / std::max(.1f, maxScale);
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
    node->setScale(std::min({maxScale, fitX, fitY}));
    node->setColor(tint);
    node->setAnchorPoint({.5f, .5f});
    node->setPosition({x, y});
    parent->addChild(node, 20);
    return node;
}

void card(CCNode* parent, float x, float y, float w, float h,
          ccColor4B fill = rgba(20, 25, 49, 250)) {
    // Rounded glass card with a dropped shadow, cool metal rim and inset face.
    roundedRect(parent, x + 5.f, y - 6.f, w, h, 12.f,
                ccc4f(.005f, .008f, .025f, .94f), 1);
    roundedRect(parent, x, y, w, h, 12.f,
                ccc4f(.43f, .53f, .82f, .96f), 2);
    roundedRect(parent, x + 1.5f, y + 1.5f, w - 3.f, h - 3.f, 10.f,
                ccc4f(.025f, .04f, .095f, .99f), 3);
    roundedRect(parent, x + 3.5f, y + 3.5f, w - 7.f, h - 7.f, 8.f,
                ccc4f(fill.r / 255.f, fill.g / 255.f, fill.b / 255.f, fill.a / 255.f), 4);

    auto glint = CCDrawNode::create();
    if (glint) {
        glint->drawSegment({x + 18.f, y + h - 7.f}, {x + w - 18.f, y + h - 7.f},
                           1.f, ccc4f(.83f, .90f, 1.f, .24f));
        glint->drawSegment({x + 10.f, y + 13.f}, {x + 10.f, y + h - 13.f},
                           .8f, ccc4f(.57f, .76f, 1.f, .20f));
        parent->addChild(glint, 5);
    }
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

void drawPill(CCDrawNode* node, float width, float height, ccColor4F color) {
    if (!node) return;
    node->clear();
    const float r = height * .5f;
    const float left = -width * .5f + r;
    const float right = width * .5f - r;
    node->drawSegment({left, 0}, {right, 0}, r, color);
    node->drawDot({left, 0}, r, color);
    node->drawDot({right, 0}, r, color);
}

void addWindFlower(CCNode* parent, float x, float y, float scale, float delay, float alpha) {
    auto plant = CCNode::create();
    if (!plant) return;
    plant->setPosition({x, y});
    plant->setAnchorPoint({0.f, 0.f});
    plant->setContentSize({1.f, 1.f});
    const float h = 68.f * scale;
    auto art = CCDrawNode::create();
    if (!art) return;

    const auto stem = ccc4f(.38f, .77f, .61f, alpha);
    const auto leaf = ccc4f(.52f, .86f, .67f, alpha * .95f);
    const auto pink = ccc4f(1.f, .42f, .77f, alpha);
    const auto lightPink = ccc4f(1.f, .72f, .90f, alpha * .94f);
    art->drawSegment({0, 0}, {0, h}, 1.5f * scale, stem);
    art->drawSegment({0, h * .32f}, {-11.f * scale, h * .47f}, 2.1f * scale, leaf);
    art->drawSegment({-11.f * scale, h * .47f}, {-16.f * scale, h * .44f}, 1.3f * scale, leaf);
    art->drawSegment({0, h * .56f}, {10.f * scale, h * .70f}, 2.1f * scale, leaf);
    art->drawSegment({10.f * scale, h * .70f}, {15.f * scale, h * .67f}, 1.3f * scale, leaf);
    for (int i = 0; i < 6; ++i) {
        const float a = i * PI / 3.f;
        art->drawDot({std::cos(a) * 6.8f * scale, h + std::sin(a) * 6.8f * scale},
                     5.5f * scale, (i % 2) ? pink : lightPink);
    }
    art->drawDot({0, h}, 3.3f * scale, ccc4f(1.f, .86f, .48f, alpha));
    plant->addChild(art);
    parent->addChild(plant, 3);

    // Alternating gusts make nearby flower heads sway like a field in a light breeze.
    plant->runAction(CCRepeatForever::create(CCSequence::create(
        CCDelayTime::create(delay),
        CCEaseSineInOut::create(CCRotateTo::create(1.35f, 4.2f)),
        CCEaseSineInOut::create(CCRotateTo::create(2.55f, -5.2f)),
        CCEaseSineInOut::create(CCRotateTo::create(1.45f, 0.f)), nullptr)));
}
}


void drawVirgoConstellation(CCNode* parent, CCPoint center, float scale, float alpha, bool showLabel) {
    if (!parent) return;
    // Stylized Virgo star map: the connecting lines trace the constellation's long Y-like form.
    const CCPoint stars[] = {
        {-78.f, 82.f}, {-26.f, 61.f}, {37.f, 76.f}, {7.f, 24.f},
        {-46.f, -13.f}, {-66.f, -67.f}, {-7.f, -82.f},
        {53.f, -112.f}, {-112.f, -118.f}, {-34.f, -145.f}
    };
    const int links[][2] = {
        {0,1},{1,2},{1,3},{3,4},{4,5},{5,6},{6,7},{5,8},{5,9}
    };
    auto lines = CCDrawNode::create();
    if (lines) {
        for (const auto& edge : links) {
            const auto a = stars[edge[0]];
            const auto b = stars[edge[1]];
            lines->drawSegment({a.x * scale, a.y * scale}, {b.x * scale, b.y * scale},
                               1.25f, ccc4f(.46f, .77f, 1.f, alpha * .72f));
        }
        lines->setPosition(center);
        parent->addChild(lines, 6);
    }

    for (int i = 0; i < 10; ++i) {
        auto halo = CCDrawNode::create();
        if (halo) {
            halo->drawDot({0, 0}, (i == 5 ? 8.2f : 5.2f) * scale,
                          ccc4f(.41f, .70f, 1.f, alpha * .13f));
            halo->setPosition({center.x + stars[i].x * scale, center.y + stars[i].y * scale});
            parent->addChild(halo, 7);
            halo->runAction(CCRepeatForever::create(CCSequence::create(
                CCEaseSineInOut::create(CCScaleTo::create(1.4f + (i % 3) * .3f, 1.25f)),
                CCEaseSineInOut::create(CCScaleTo::create(1.4f + (i % 3) * .3f, .82f)), nullptr)));
        }
        auto point = CCDrawNode::create();
        if (point) {
            point->drawDot({0, 0}, (i == 5 ? 3.6f : 2.5f) * scale,
                           ccc4f(.83f, .93f, 1.f, alpha));
            point->setPosition({center.x + stars[i].x * scale, center.y + stars[i].y * scale});
            parent->addChild(point, 8);
        }
    }

    if (showLabel) {
        auto label = CCLabelBMFont::create("VIRGO  /  CONSTELLATION", "bigFont.fnt");
        if (label) {
            label->setScale(.38f * scale / .58f);
            label->setColor(ccc3(151, 193, 238));
            label->setPosition({center.x, center.y - 96.f * scale});
            parent->addChild(label, 8);
        }
    }
}

class FloweristOverlay final : public CCLayer {
    struct Hit {
        int tag;
        CCPoint center;
        float width;
        float height;
        CCNode* space;
        CCDrawNode* plate;
        CCDrawNode* glow;
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
        // True horizontal capsules: wide oval silhouette, inset rim, soft under-shadow.
        ccColor3B normal = style == 1 ? ccc3(31, 112, 101) :
                           style == 2 ? ccc3(117, 48, 83) :
                           style == 3 ? ccc3(75, 91, 164) : ccc3(20, 35, 64);
        ccColor3B hot = style == 1 ? ccc3(54, 222, 177) :
                        style == 2 ? ccc3(255, 105, 174) :
                        style == 3 ? ccc3(127, 153, 255) : ccc3(78, 178, 239);

        auto shadow = CCDrawNode::create();
        if (shadow) {
            shadow->setPosition({x + 2.f, y - 3.5f});
            drawPill(shadow, w + 7.f, h + 7.f, ccc4f(.005f, .008f, .025f, .96f));
            parent->addChild(shadow, 7);
        }

        auto aura = CCDrawNode::create();
        if (aura) {
            aura->setPosition({x, y});
            drawPill(aura, w + 16.f, h + 13.f,
                     ccc4f(hot.r / 255.f, hot.g / 255.f, hot.b / 255.f, .22f));
            aura->setVisible(false);
            parent->addChild(aura, 8);
        }

        auto rim = CCDrawNode::create();
        if (rim) {
            rim->setPosition({x, y});
            drawPill(rim, w + 4.f, h + 4.f, ccc4f(.60f, .68f, .98f, .98f));
            parent->addChild(rim, 9);
        }

        auto plate = CCDrawNode::create();
        if (plate) {
            plate->setPosition({x, y});
            drawPill(plate, w, h,
                     ccc4f(normal.r / 255.f, normal.g / 255.f, normal.b / 255.f, .99f));
            parent->addChild(plate, 10);
        }

        auto shine = CCDrawNode::create();
        if (shine) {
            shine->setPosition({x, y});
            // A short, curved-looking highlight across the top third of the capsule.
            shine->drawSegment({-w * .31f, h * .23f}, {w * .31f, h * .23f}, 1.05f,
                               ccc4f(.94f, .96f, 1.f, .42f));
            parent->addChild(shine, 11);
        }

        const bool isTab = !page && !modal && tag >= 0 && tag < TAB_COUNT;
        text(parent, caption, x, y - .2f, w - 20.f, h * .60f,
             ccc3(246, 249, 255), "bigFont.fnt", isTab ? .78f : .44f);
        m_hits.push_back({tag, {x, y}, w, h, parent, plate, aura, normal, hot, page, modal});
    }

    void updateNav() {
        for (auto& hit : m_hits) {
            if (hit.page || hit.modal || hit.space != m_panel || hit.tag < 0 || hit.tag >= TAB_COUNT) continue;
            const bool selected = hit.tag == m_selected;
            hit.normal = selected ? ccc3(63, 91, 155) : ccc3(20, 35, 64);
            hit.hot = selected ? ccc3(133, 157, 255) : ccc3(80, 185, 244);
            if (hit.plate) {
                const auto col = hit.normal;
                drawPill(hit.plate, hit.width, hit.height,
                    ccc4f(col.r / 255.f, col.g / 255.f, col.b / 255.f, .99f));
            }
        }
    }

    void drawBackdrop() {
        const auto win = CCDirector::sharedDirector()->getWinSize();
        const float w = win.width;
        const float h = win.height;

        // Deep-space base with layered violet/cyan nebulae. No blocky colour slabs.
        rect(this, 0, 0, w, h, rgba(4, 7, 20, 255), -20);

        auto makeNebula = [this](CCPoint at, float size, ccColor4F core, ccColor4F halo,
                                 float driftX, float driftY, float duration) {
            auto cloud = CCNode::create();
            if (!cloud) return;
            auto glow = CCDrawNode::create();
            if (!glow) return;
            glow->drawDot({0, 0}, size, halo);
            glow->drawDot({-size * .16f, size * .06f}, size * .68f, core);
            glow->drawDot({size * .19f, -size * .11f}, size * .39f,
                          ccc4f(core.r, core.g, core.b, core.a * .72f));
            cloud->addChild(glow);
            cloud->setPosition(at);
            addChild(cloud, -16);
            cloud->runAction(CCRepeatForever::create(CCSequence::create(
                CCEaseSineInOut::create(CCMoveBy::create(duration, {driftX, driftY})),
                CCEaseSineInOut::create(CCMoveBy::create(duration, {-driftX, -driftY})), nullptr)));
            glow->runAction(CCRepeatForever::create(CCSequence::create(
                CCEaseSineInOut::create(CCScaleTo::create(duration * .72f, 1.08f)),
                CCEaseSineInOut::create(CCScaleTo::create(duration * .72f, .96f)), nullptr)));
        };
        makeNebula({w * .17f, h * .78f}, std::min(w, h) * .28f,
                   ccc4f(.23f, .15f, .48f, .075f), ccc4f(.18f, .31f, .78f, .045f), 11.f, -5.f, 9.f);
        makeNebula({w * .78f, h * .72f}, std::min(w, h) * .31f,
                   ccc4f(.29f, .13f, .48f, .080f), ccc4f(.55f, .18f, .49f, .045f), -13.f, 6.f, 11.f);
        makeNebula({w * .53f, h * .22f}, std::min(w, h) * .23f,
                   ccc4f(.08f, .31f, .49f, .055f), ccc4f(.10f, .22f, .62f, .035f), 8.f, 4.f, 10.f);

        // Sparse star field: varied sizes and independent pulse timings create depth.
        for (int i = 0; i < 112; ++i) {
            const float x = ((i * 193 + 71) % 997) / 997.f * w;
            const float y = ((i * 127 + 43) % 991) / 991.f * h;
            const float radius = (i % 17 == 0) ? 2.0f : ((i % 5 == 0) ? 1.25f : .72f);
            auto starNode = CCDrawNode::create();
            if (!starNode) continue;
            const auto tint = (i % 4 == 0) ? ccc4f(.60f, .83f, 1.f, .88f) :
                              (i % 7 == 0) ? ccc4f(.95f, .69f, 1.f, .86f) :
                                            ccc4f(.83f, .88f, 1.f, .78f);
            starNode->drawDot({0, 0}, radius, tint);
            if (i % 17 == 0) {
                starNode->drawSegment({-radius * 2.7f, 0}, {radius * 2.7f, 0}, .55f,
                                      ccc4f(tint.r, tint.g, tint.b, .55f));
                starNode->drawSegment({0, -radius * 2.7f}, {0, radius * 2.7f}, .55f,
                                      ccc4f(tint.r, tint.g, tint.b, .55f));
            }
            starNode->setPosition({x, y});
            addChild(starNode, -10);
            starNode->runAction(CCRepeatForever::create(CCSequence::create(
                CCDelayTime::create((i % 13) * .11f),
                CCEaseSineInOut::create(CCScaleTo::create(1.0f + (i % 4) * .25f, 1.35f)),
                CCEaseSineInOut::create(CCScaleTo::create(1.0f + (i % 4) * .25f, .78f)), nullptr)));
        }

        // Distant ringed planet gives the space scene a clear focal point.
        auto planet = CCDrawNode::create();
        if (planet) {
            planet->drawDot({0, 0}, 28.f, ccc4f(.16f, .24f, .53f, .96f));
            planet->drawDot({-8, 10}, 12.f, ccc4f(.50f, .55f, .94f, .32f));
            planet->drawDot({8, -9}, 8.f, ccc4f(.05f, .10f, .28f, .44f));
            planet->setPosition({w * .12f, h * .22f});
            addChild(planet, -5);
            auto ring = orbitalRing(this, {w * .12f, h * .22f}, 51.f, 15.f, -4);
            if (ring) {
                ring->setRotation(-19.f);
                ring->runAction(CCRepeatForever::create(CCSequence::create(
                    CCRotateBy::create(18.f, 5.f), CCRotateBy::create(18.f, -5.f), nullptr)));
            }
            planet->runAction(CCRepeatForever::create(CCSequence::create(
                CCEaseSineInOut::create(CCMoveBy::create(4.8f, {0, 3.f})),
                CCEaseSineInOut::create(CCMoveBy::create(4.8f, {0, -3.f})), nullptr)));
        }

        // Small moving light trails. They drift slowly and loop without a hard flash.
        for (int i = 0; i < 4; ++i) {
            auto trail = CCDrawNode::create();
            if (!trail) continue;
            trail->drawSegment({-28.f, 0}, {0.f, 0}, 1.2f,
                               i % 2 ? ccc4f(.63f, .79f, 1.f, .50f) : ccc4f(.90f, .70f, 1.f, .48f));
            trail->setPosition({w * (.22f + .19f * i), h * (.68f + .055f * (i % 3))});
            trail->setRotation(-17.f);
            addChild(trail, -8);
            trail->runAction(CCRepeatForever::create(CCSequence::create(
                CCDelayTime::create(i * 1.1f),
                CCEaseSineInOut::create(CCMoveBy::create(3.4f, {34.f, -12.f})),
                CCEaseSineInOut::create(CCMoveBy::create(3.4f, {-34.f, 12.f})), nullptr)));
        }

        // A faint zodiac constellation is also visible in the open sky around the panel.
        drawVirgoConstellation(this, {w * .79f, h * .83f}, .48f, .38f, false);
    }

    void decorate() {
        // Premium sci-fi frame: layered graphite glass, thin violet/cyan metal rim.
        roundedRect(m_panel, 0, 0, PANEL_W, PANEL_H, 15.f, ccc4f(.38f, .48f, .77f, .97f), 0);
        roundedRect(m_panel, 1.5f, 1.5f, PANEL_W - 3.f, PANEL_H - 3.f, 13.f, ccc4f(.018f, .028f, .072f, .99f), 1);
        roundedRect(m_panel, 4.f, 4.f, PANEL_W - 8.f, PANEL_H - 8.f, 11.f, ccc4f(.047f, .067f, .14f, .98f), 2);
        rect(m_panel, 7.f, 7.f, NAV_W, PANEL_H - 14.f, rgba(8, 13, 31, 252), 3);
        rect(m_panel, NAV_W + 7.f, 8.f, 1.2f, PANEL_H - 16.f, rgba(90, 150, 212, 160), 4);

        auto trim = CCDrawNode::create();
        if (trim) {
            trim->drawSegment({18, PANEL_H - 4}, {PANEL_W - 18, PANEL_H - 4}, 1.15f,
                              ccc4f(.58f, .78f, 1.f, .68f));
            trim->drawSegment({18, 4}, {PANEL_W - 18, 4}, .8f,
                              ccc4f(.47f, .58f, .91f, .35f));
            m_panel->addChild(trim, 5);
        }

        // Right-hand glass surface: a single continuous tone instead of striped slabs.
        auto field = CCNode::create();
        m_panel->addChild(field, 3);
        const float fieldX = NAV_W + 9.f;
        const float fieldW = PANEL_W - fieldX - 8.f;
        const float fieldY = 8.f;
        const float fieldH = PANEL_H - 16.f;
        rect(field, fieldX, fieldY, fieldW, fieldH, rgba(8, 12, 30, 218), 0);

        // Large low-opacity nebula blooms create depth behind the content without obscuring it.
        auto nebulaOne = CCDrawNode::create();
        if (nebulaOne) {
            nebulaOne->drawDot({0, 0}, 116.f, ccc4f(.27f, .23f, .66f, .075f));
            nebulaOne->drawDot({18, 12}, 72.f, ccc4f(.21f, .54f, .80f, .045f));
            nebulaOne->setPosition({fieldX + 375.f, 312.f});
            field->addChild(nebulaOne, 1);
            nebulaOne->runAction(CCRepeatForever::create(CCSequence::create(
                CCEaseSineInOut::create(CCScaleTo::create(4.5f, 1.07f)),
                CCEaseSineInOut::create(CCScaleTo::create(4.5f, .96f)), nullptr)));
        }
        auto nebulaTwo = CCDrawNode::create();
        if (nebulaTwo) {
            nebulaTwo->drawDot({0, 0}, 94.f, ccc4f(.46f, .18f, .53f, .055f));
            nebulaTwo->drawDot({-14, 9}, 52.f, ccc4f(.16f, .38f, .73f, .045f));
            nebulaTwo->setPosition({fieldX + 108.f, 105.f});
            field->addChild(nebulaTwo, 1);
            nebulaTwo->runAction(CCRepeatForever::create(CCSequence::create(
                CCEaseSineInOut::create(CCScaleTo::create(5.2f, 1.06f)),
                CCEaseSineInOut::create(CCScaleTo::create(5.2f, .95f)), nullptr)));
        }

        // Fine stars across the panel's open areas; cards sit above them.
        for (int i = 0; i < 35; ++i) {
            auto point = CCDrawNode::create();
            if (!point) continue;
            const float radius = (i % 9 == 0) ? 1.65f : .85f;
            point->drawDot({0, 0}, radius, (i % 4 == 0)
                ? ccc4f(.68f, .83f, 1.f, .60f) : ccc4f(.83f, .86f, 1.f, .40f));
            point->setPosition({fieldX + 18.f + static_cast<float>((i * 79 + 31) % 480),
                                22.f + static_cast<float>((i * 53 + 17) % 390)});
            field->addChild(point, 2);
            point->runAction(CCRepeatForever::create(CCSequence::create(
                CCDelayTime::create((i % 8) * .16f),
                CCEaseSineInOut::create(CCScaleTo::create(1.2f, 1.45f)),
                CCEaseSineInOut::create(CCScaleTo::create(1.2f, .78f)), nullptr)));
        }

        // The Virgo constellation is a signature element in the open upper-right sky.
        drawVirgoConstellation(field, {fieldX + fieldW * .80f, 298.f}, .43f, .82f, false);

        // A few slow drifting particles give the header area subtle motion.
        for (int i = 0; i < 12; ++i) {
            auto particle = CCDrawNode::create();
            if (!particle) continue;
            particle->drawDot({0, 0}, 1.25f + (i % 3) * .28f,
                              ccc4f(.63f, .83f, 1.f, .55f));
            particle->setPosition({fieldX + 25.f + static_cast<float>((i * 43) % 440),
                                   255.f + static_cast<float>((i * 19) % 90)});
            field->addChild(particle, 3);
            particle->runAction(CCRepeatForever::create(CCSequence::create(
                CCEaseSineInOut::create(CCMoveBy::create(3.5f + (i % 3), {8.f, 5.f})),
                CCEaseSineInOut::create(CCMoveBy::create(3.5f + (i % 3), {-8.f, -5.f})), nullptr)));
        }

        // Brand lockup: a small astronomical sigil, no flower icon or old project name.
        auto sigil = CCDrawNode::create();
        if (sigil) {
            sigil->drawSegment({-10, 0}, {0, 13}, 2.0f, ccc4f(.65f, .82f, 1.f, .96f));
            sigil->drawSegment({0, 13}, {10, 0}, 2.0f, ccc4f(.65f, .82f, 1.f, .96f));
            sigil->drawSegment({10, 0}, {0, -13}, 2.0f, ccc4f(.77f, .60f, 1.f, .96f));
            sigil->drawSegment({0, -13}, {-10, 0}, 2.0f, ccc4f(.77f, .60f, 1.f, .96f));
            sigil->drawDot({0, 0}, 3.2f, ccc4f(.91f, .94f, 1.f, 1.f));
            sigil->setPosition({31, PANEL_H - 36});
            m_panel->addChild(sigil, 6);
            sigil->runAction(CCRepeatForever::create(CCSequence::create(
                CCEaseSineInOut::create(CCScaleTo::create(2.1f, 1.08f)),
                CCEaseSineInOut::create(CCScaleTo::create(2.1f, .94f)), nullptr)));
        }
        text(m_panel, "VIRGO", 100, PANEL_H - 27, 126, 24,
             ccc3(243, 247, 255), "goldFont.fnt", .78f);
        text(m_panel, "PREDICTIVE INPUT SYSTEM", 100, PANEL_H - 49, 126, 11,
             ccc3(129, 185, 235), "bigFont.fnt", .46f);
        text(m_panel, "BUILD 02  /  60 TICK CAP", 100, PANEL_H - 72, 126, 9,
             ccc3(104, 139, 191), "bigFont.fnt", .40f);

        // Spacious, consistently aligned capsule navigation. Tab typography stays large.
        for (int i = 0; i < TAB_COUNT; ++i) {
            const float y = PANEL_H - 120.f - i * 49.f;
            button(m_panel, TAB_NAMES[i], 91, y, 156, 27, i, false, false, i == m_selected ? 3 : 0);
        }
        button(m_panel, "GUIDE", 91, 33, 156, 27, 110);
        text(m_panel, "F2  MENU / CLOSE", 91, 12, 140, 9, ccc3(94, 133, 186), "bigFont.fnt", .40f);
        button(m_panel, "X", PANEL_W - 24, PANEL_H - 25, 34, 30, 100);
        text(m_panel, "ONLINE", PANEL_W - 95, PANEL_H - 28, 68, 12,
             ccc3(85, 236, 190), "bigFont.fnt", .40f);
        updateNav();
    }

    void sectionTitle(const std::string& title, const std::string& subtitle) {
        text(m_content, "VIRGO  /  SPACE CONTROL", 430, 414, 450, 10,
             ccc3(117, 163, 220), "bigFont.fnt", .43f);
        text(m_content, title, 430, 390, 450, 27,
             ccc3(238, 220, 255), "goldFont.fnt", .70f);
        text(m_content, subtitle, 430, 365, 455, 21,
             ccc3(145, 170, 220), "bigFont.fnt", .43f);
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
            text(m_content, "Open the log console when needed.",
                 232, 135, 255, 24, ccc3(136, 156, 205), "bigFont.fnt", .42f);
            button(m_content, s_consoleVisible ? "CLOSE CONSOLE" : "OPEN CONSOLE",
                   555, 143, 164, 32, 103, true);
        } else if (m_selected == 1) {
            const char* names[] = {"LOW-END", "BALANCED", "CUSTOM"};
            const char* detail[] = {"Prioritizes a lighter interface", "A balanced default profile", "A manually configured profile"};
            for (int i = 0; i < 3; ++i) {
                const float x = 270.f + i * 160.f;
                card(m_content, x - 73, 169, 146, 148,
                     i == m_selectedMode ? rgba(43, 35, 80, 255) : rgba(22, 26, 54, 255));
                text(m_content, names[i], x, 279, 130, 18,
                     i == m_selectedMode ? ccc3(232, 211, 255) : ccc3(177, 188, 226));
                text(m_content, detail[i], x, 239, 124, 38, ccc3(145, 162, 214));
                button(m_content, i == m_selectedMode ? "SELECTED" : "SELECT",
                       x, 192, 122, 32, 210 + i, true, false, i == m_selectedMode ? 3 : 0);
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
                const float x = 270.f + col * 160.f;
                const float y = row == 0 ? 244.f : 136.f;
                card(m_content, x - 73, y - 40, 146, 92,
                     i == m_selectedStage ? rgba(52, 39, 90, 255) : fills[i]);
                text(m_content, names[i], x, y + 19, 130, 17,
                     i == m_selectedStage ? ccc3(238, 214, 255) : ccc3(184, 194, 233));
                text(m_content, detail[i], x, y - 3, 122, 13, ccc3(145, 166, 220));
                button(m_content, i == m_selectedStage ? "ACTIVE" : "CHOOSE", x, y - 27,
                       118, 25, 200 + i, true, false, i == m_selectedStage ? 3 : 0);
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
            // CCNode has no opacity API in this Cocos2d build; use a subtle scale-in.
            m_content->setScale(.985f);
            m_content->runAction(CCEaseSineOut::create(CCScaleTo::create(.18f, 1.f)));
        }
    }

    void drawLOK() {
        // LOK is the custom VIRGO guide mascot. Keep the character prominent
        // on the left and use a compact speech capsule that never covers her.
        auto portrait = CCNode::create();
        portrait->setPosition({238, 218});
        m_modalLayer->addChild(portrait, 30);

        auto orbit = CCDrawNode::create();
        orbit->drawSegment({-48, 0}, {48, 0}, 1.2f, ccc4f(.67f, .61f, 1.f, .33f));
        orbit->drawSegment({0, -38}, {0, 38}, 1.0f, ccc4f(.67f, .61f, 1.f, .20f));
        orbit->drawDot({-43, 0}, 3.f, ccc4f(.63f, .76f, 1.f, .86f));
        orbit->drawDot({43, 0}, 3.f, ccc4f(1.f, .55f, .81f, .86f));
        portrait->addChild(orbit, 0);
        if (m_animated) orbit->runAction(CCRepeatForever::create(CCRotateBy::create(15.f, 360.f)));

        auto aura = CCDrawNode::create();
        aura->drawDot({0, 0}, 54, ccc4f(.61f, .31f, .94f, .13f));
        aura->drawDot({0, 0}, 42, ccc4f(.95f, .40f, .77f, .10f));
        portrait->addChild(aura, 1);
        if (m_animated) aura->runAction(CCRepeatForever::create(CCSequence::create(
            CCEaseSineInOut::create(CCScaleTo::create(1.2f, 1.07f)),
            CCEaseSineInOut::create(CCScaleTo::create(1.2f, .95f)), nullptr)));

        auto shadow = CCDrawNode::create();
        shadow->drawDot({0, -25}, 28, ccc4f(.02f, .02f, .09f, .38f));
        portrait->addChild(shadow, 2);

        auto body = CCDrawNode::create();
        const auto shell = ccc4f(.48f, .69f, .98f, 1.f);
        const auto shade = ccc4f(.27f, .39f, .78f, 1.f);
        body->drawSegment({-18, 12}, {18, 12}, 22, shell);
        body->drawSegment({-27, -3}, {27, -3}, 25, shell);
        body->drawSegment({-22, -14}, {22, -14}, 17, shell);
        body->drawDot({-26, -3}, 15, shell);
        body->drawDot({26, -3}, 15, shell);
        body->drawDot({0, 3}, 34, shell);
        body->drawSegment({-19, 21}, {-25, 31}, 6, shade);
        body->drawSegment({19, 21}, {25, 31}, 6, shade);
        body->drawSegment({-14, 25}, {13, 25}, 2.5f, ccc4f(1.f, .91f, .98f, .72f));
        portrait->addChild(body, 3);

        auto leftEye = CCDrawNode::create();
        leftEye->drawDot({0, 0}, 4.7f, ccc4f(.17f, .14f, .32f, 1.f));
        leftEye->setPosition({-12, 5});
        portrait->addChild(leftEye, 5);
        auto rightEye = CCDrawNode::create();
        rightEye->drawDot({0, 0}, 4.7f, ccc4f(.17f, .14f, .32f, 1.f));
        rightEye->setPosition({12, 5});
        portrait->addChild(rightEye, 5);
        if (m_animated) {
            auto blink = [] (CCNode* eye, float delay) {
                eye->runAction(CCRepeatForever::create(CCSequence::create(
                    CCDelayTime::create(delay), CCScaleTo::create(.07f, 1.f, .10f),
                    CCScaleTo::create(.10f, 1.f, 1.f), CCDelayTime::create(2.5f), nullptr)));
            };
            blink(leftEye, 1.15f);
            blink(rightEye, 1.15f);
        }

        auto expression = CCDrawNode::create();
        expression->drawDot({-21, -8}, 4.8f, ccc4f(.54f, .78f, 1.f, .52f));
        expression->drawDot({21, -8}, 4.8f, ccc4f(.54f, .78f, 1.f, .52f));
        expression->drawSegment({-5, -12}, {0, -15}, 1.8f, ccc4f(.43f, .15f, .36f, 1.f));
        expression->drawSegment({0, -15}, {5, -12}, 1.8f, ccc4f(.43f, .15f, .36f, 1.f));
        portrait->addChild(expression, 5);

        auto crest = CCNode::create();
        crest->setPosition({0, 40});
        portrait->addChild(crest, 6);
        auto zodiacMark = CCDrawNode::create();
        if (zodiacMark) {
            zodiacMark->drawSegment({-9, 0}, {-3, 10}, 2.f, ccc4f(.72f, .88f, 1.f, 1.f));
            zodiacMark->drawSegment({-3, 10}, {4, -1}, 2.f, ccc4f(.72f, .88f, 1.f, 1.f));
            zodiacMark->drawSegment({4, -1}, {9, 9}, 2.f, ccc4f(.72f, .88f, 1.f, 1.f));
            zodiacMark->drawSegment({9, 9}, {8, -8}, 2.f, ccc4f(.72f, .88f, 1.f, 1.f));
            zodiacMark->drawSegment({8, -8}, {1, -11}, 2.f, ccc4f(.72f, .88f, 1.f, 1.f));
            zodiacMark->drawDot({0, 0}, 2.4f, ccc4f(.96f, .97f, 1.f, 1.f));
            crest->addChild(zodiacMark);
        }
        if (m_animated) {
            crest->runAction(CCRepeatForever::create(CCSequence::create(
                CCRotateBy::create(6.f, 360.f), nullptr)));
            portrait->runAction(CCRepeatForever::create(CCSequence::create(
                CCEaseSineInOut::create(CCMoveBy::create(.90f, {0, 3.5f})),
                CCEaseSineInOut::create(CCMoveBy::create(.90f, {0, -3.5f})), nullptr)));
            body->runAction(CCRepeatForever::create(CCSequence::create(
                CCEaseSineInOut::create(CCScaleTo::create(1.3f, 1.025f)),
                CCEaseSineInOut::create(CCScaleTo::create(1.3f, 1.f)), nullptr)));
        }

        // A compact oval speech bubble leaves a clear gap between the text and LOK.
        auto bubble = CCNode::create();
        bubble->setPosition({444, 207});
        m_modalLayer->addChild(bubble, 31);
        auto bubbleShape = CCDrawNode::create();
        bubbleShape->setPosition({0, 0});
        drawPill(bubbleShape, 174.f, 86.f, ccc4f(.10f, .12f, .25f, .98f));
        bubble->addChild(bubbleShape, 0);
        auto bubbleRim = CCDrawNode::create();
        drawPill(bubbleRim, 178.f, 90.f, ccc4f(.64f, .51f, .91f, .80f));
        // Dark inner capsule sits over the thin violet outline.
        auto inner = CCDrawNode::create();
        drawPill(inner, 172.f, 84.f, ccc4f(.10f, .12f, .25f, .99f));
        bubble->addChild(bubbleRim, -1);
        bubble->addChild(inner, 0);
        auto tail = CCDrawNode::create();
        tail->drawSegment({-84, 4}, {-101, 13}, 2.2f, ccc4f(.72f, .61f, .95f, .92f));
        bubble->addChild(tail, 1);

        text(bubble, "LOK  /  VIRGO ASSISTANT", 0, 22, 146, 12,
             ccc3(225, 197, 255), "bigFont.fnt", .42f);
        static const char* lines[] = {
            "Hi! I'm LOK, your VIRGO assistant!",
            "Use the left rail to switch tabs.",
            "Choose a stage card to begin.",
            "The console opens only on request.",
            "You're all set. Enjoy VIRGO!"
        };
        m_guideSpeech = text(bubble, lines[m_guideStep], 0, -7, 146, 34,
                             ccc3(232, 232, 250), "bigFont.fnt", .46f);
        if (m_animated) {
            bubble->setScale(.90f);
            bubble->runAction(CCEaseBackOut::create(CCScaleTo::create(.22f, 1.f)));
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
        card(m_modalLayer, 128, 82, 448, 284, rgba(22, 22, 52, 255));
        if (m_modal == 1) {
            text(m_modalLayer, "QUICK HELP", 352, 318, 300, 22, ccc3(226, 198, 255), "goldFont.fnt", .68f);
            text(m_modalLayer, "Do you want a guide about the GUI?", 352, 267, 330, 20, ccc3(237, 237, 255), "bigFont.fnt", .44f);
            text(m_modalLayer, "LOK appears only if you choose YES.", 352, 231, 320, 14, ccc3(151, 170, 218), "bigFont.fnt", .42f);
            button(m_modalLayer, "YES", 268, 174, 118, 38, 111, false, true, 1);
            button(m_modalLayer, "NO", 436, 174, 118, 38, 112, false, true, 2);
        } else {
            text(m_modalLayer, "VIRGO INTERFACE TOUR", 352, 338, 360, 22,
                 ccc3(226, 198, 255), "goldFont.fnt", .68f);
            drawLOK();
            button(m_modalLayer, m_guideStep == 4 ? "FINISH" : "NEXT", 414, 112, 104, 29, 113, false, true, 3);
            button(m_modalLayer, "CLOSE", 532, 112, 82, 29, 114, false, true);
            text(m_modalLayer, std::to_string(m_guideStep + 1) + " / 5", 230, 112, 70, 12, ccc3(146, 164, 213));
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
                    SetConsoleTitleA("VIRGO - Console");
                    s_consoleVisible = true;
                    printf("[VIRGO] Console opened from the LOG tab.\n");
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
                if (hit.plate) {
                    const auto color = hot ? hit.hot : hit.normal;
                    drawPill(hit.plate, hit.width, hit.height,
                        ccc4f(color.r / 255.f, color.g / 255.f, color.b / 255.f, .98f));
                }
                if (hit.glow) hit.glow->setVisible(hot);
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


