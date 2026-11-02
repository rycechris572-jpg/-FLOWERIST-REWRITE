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
#include "VirgoShared.hpp"

using namespace geode::prelude;

namespace {
constexpr float PANEL_W = 704.f;
constexpr float PANEL_H = 448.f;
constexpr float NAV_W = 174.f;
constexpr float PI = 3.1415926535f;
constexpr int TAB_COUNT = 6;
bool s_consoleVisible = false;
const char* TAB_NAMES[TAB_COUNT] = {"LOGS", "STAGES", "MODES", "ASSISTS", "OPTIMIZATION", "BRAIN / AUTO"};
const char* TAB_INFO[TAB_COUNT] = {
    "System status and recent activity",
    "Choose a stage and tune Cube, Wave and Ship",
    "Choose which gamemodes use Predictive Input Buffer",
    "See the active assistance systems",
    "Tune the interface and performance",
    "Persistent learning records and level-aware autoplay"
};

ccColor4B rgba(int r, int g, int b, int a = 255) { return ccc4(r, g, b, a); }

CCLayerColor* rect(CCNode* parent, float x, float y, float w, float h, ccColor4B c, int z = 0) {
    auto node = CCLayerColor::create(c, w, h);
    if (!node) return nullptr;
    node->setPosition({x, y});
    parent->addChild(node, z);
    return node;
}

std::vector<CCPoint> roundedPoints(float width, float height, float radius, int steps = 8) {
    std::vector<CCPoint> points;
    const float r = std::min(radius, std::min(width, height) * .5f);
    const float halfW = width * .5f;
    const float halfH = height * .5f;
    const CCPoint centers[] = {
        { halfW - r, -halfH + r },
        { halfW - r,  halfH - r },
        {-halfW + r,  halfH - r },
        {-halfW + r, -halfH + r }
    };
    const float starts[] = {-90.f, 0.f, 90.f, 180.f};
    points.reserve(static_cast<size_t>(4 * (steps + 1)));
    for (int corner = 0; corner < 4; ++corner) {
        for (int i = 0; i <= steps; ++i) {
            const float angle = (starts[corner] + 90.f * static_cast<float>(i) / steps)
                              * 3.1415926535f / 180.f;
            points.push_back({centers[corner].x + std::cos(angle) * r,
                              centers[corner].y + std::sin(angle) * r});
        }
    }
    return points;
}

void roundedRect(CCNode* parent, float x, float y, float w, float h, float radius,
                 ccColor4F color, int z = 0) {
    // Filled rounded polygon. The old line-only construction left card interiors
    // transparent, allowing background effects to bleed through as square blocks.
    auto node = CCDrawNode::create();
    if (!node) return;
    node->setPosition({x + w * .5f, y + h * .5f});
    auto points = roundedPoints(w, h, radius, 10);
    node->drawPolygon(points.data(), static_cast<unsigned int>(points.size()),
                      color, 0.f, ccc4f(0.f, 0.f, 0.f, 0.f));
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
    // Use a filled polygon with a semicircular end-cap, not a stroked rectangle.
    auto points = roundedPoints(width, height, height * .5f, 16);
    node->drawPolygon(points.data(), static_cast<unsigned int>(points.size()),
                      color, 0.f, ccc4f(0.f, 0.f, 0.f, 0.f));
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

void drawGamemodeIcon(CCNode* parent, int mode, CCPoint at, ccColor4F tint) {
    auto icon = CCDrawNode::create();
    if (!icon) return;
    const float t = 2.0f;
auto line = [&](CCPoint a, CCPoint b, float width = 2.0f) {
        icon->drawSegment(a, b, width, tint);
    };
    switch (mode) {
        case 0: // Cube: thin isometric wireframe, not a filled block.
            line({0, 12}, {-10, 5}); line({-10, 5}, {0, -2});
            line({0, -2}, {10, 5}); line({10, 5}, {0, 12});
            line({0, 12}, {0, -2}); line({-10, 5}, {0, 8}); line({10, 5}, {0, 8});
            break;
        case 1: // Wave: clean zig-zag trajectory.
            line({-13, -7}, {-5, 7}, 2.5f); line({-5, 7}, {3, -7}, 2.5f);
            line({3, -7}, {11, 7}, 2.5f); break;
        case 2: // Ship: sleek flight arrow.
            line({-13, -5}, {12, 0}, 2.3f); line({12, 0}, {-7, 8}, 2.3f);
            line({-7, 8}, {-2, 1}, 2.0f); line({-2, 1}, {-13, -5}, 2.0f);
            break;
        case 3: // Ball: circular orb with a latitude arc.
            for (int i = 0; i < 32; ++i) {
                const float a = i * 6.2831853f / 32.f;
                const float b = (i + 1) * 6.2831853f / 32.f;
                line({std::cos(a) * 10.f, std::sin(a) * 10.f},
                     {std::cos(b) * 10.f, std::sin(b) * 10.f}, 1.5f);
            }
            line({-8, 0}, {8, 0}, 1.2f); break;
        case 4: // UFO: saucer and small dome.
            line({-13, -3}, {13, -3}, 2.2f); line({-13, -3}, {-8, -7}, 1.8f);
            line({-8, -7}, {8, -7}, 1.8f); line({8, -7}, {13, -3}, 1.8f);
            line({-7, -2}, {-4, 5}, 1.8f); line({-4, 5}, {4, 5}, 1.8f);
            line({4, 5}, {7, -2}, 1.8f); break;
        case 5: // Robot: round head, antenna and eyes.
            line({-8, -7}, {-10, -2}, 1.8f); line({-10, -2}, {-8, 7}, 1.8f);
            line({-8, 7}, {8, 7}, 1.8f); line({8, 7}, {10, -2}, 1.8f);
            line({10, -2}, {8, -7}, 1.8f); line({-8, -7}, {8, -7}, 1.8f);
            line({0, 7}, {0, 12}, 1.6f); icon->drawDot({0, 14}, 2.f, tint);
            icon->drawDot({-4, 1}, 1.6f, tint); icon->drawDot({4, 1}, 1.6f, tint);
            break;
        case 6: // Spider: central core and eight fine legs.
            icon->drawDot({0, 0}, 3.2f, tint);
            for (int side : {-1, 1}) {
                line({side * 2.f, 2.f}, {side * 8.f, 9.f}, 1.6f);
                line({side * 5.f, 1.f}, {side * 12.f, 3.f}, 1.6f);
                line({side * 5.f, -1.f}, {side * 12.f, -4.f}, 1.6f);
                line({side * 2.f, -2.f}, {side * 8.f, -10.f}, 1.6f);
            }
            break;
        case 7: // Swing: tethered orb in motion.
            icon->drawDot({0, 11}, 2.4f, tint); line({0, 11}, {7, -1}, 1.8f);
            for (int i = 0; i < 24; ++i) {
                const float a = (-.2f + i * 1.25f / 24.f);
                const float b = (-.2f + (i + 1) * 1.25f / 24.f);
                line({7.f + std::cos(a) * 5.f, -1.f + std::sin(a) * 5.f},
                     {7.f + std::cos(b) * 5.f, -1.f + std::sin(b) * 5.f}, 1.5f);
            }
            break;
    }
    icon->setPosition(at);
    parent->addChild(icon, 7);
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

    void frameControl(CCNode* parent, const char* label, int modeId, float x, float y) {
        const ccColor4F accents[] = {
            ccc4f(.39f, .67f, 1.f, 1.f),
            ccc4f(.72f, .51f, 1.f, 1.f),
            ccc4f(.35f, .84f, .91f, 1.f)
        };
        const auto accent = accents[modeId];
        auto shadow = CCDrawNode::create();
        if (shadow) {
            shadow->setPosition({x + 2.f, y - 3.f});
            drawPill(shadow, 158.f, 36.f, ccc4f(.002f, .004f, .018f, .95f));
            parent->addChild(shadow, 6);
        }
        auto rim = CCDrawNode::create();
        if (rim) {
            rim->setPosition({x, y});
            drawPill(rim, 156.f, 34.f, ccc4f(accent.r, accent.g, accent.b, .88f));
            parent->addChild(rim, 7);
        }
        auto plate = CCDrawNode::create();
        if (plate) {
            plate->setPosition({x, y});
            drawPill(plate, 152.f, 30.f, ccc4f(.035f, .055f, .12f, .99f));
            parent->addChild(plate, 8);
        }
        text(parent, label, x - 43.f, y, 57.f, 18.f,
             ccc3(224, 233, 255), "bigFont.fnt", .40f);
        text(parent, std::to_string(virgo::modeFrames[modeId]) + "F", x + 5.f, y,
             42.f, 18.f, ccc3(248, 250, 255), "goldFont.fnt", .48f);
        button(parent, "-", x + 46.f, y, 19.f, 21.f, 220 + modeId * 2,
               true, false, 3);
        button(parent, "+", x + 67.f, y, 19.f, 21.f, 221 + modeId * 2,
               true, false, 3);
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

        // The space scene stays outside the control surface. No translucent nebula
        // blooms or large particles are placed behind the cards or text.
        rect(this, 0, 0, w, h, rgba(3, 6, 18, 255), -20);

        // Restrained star points are visible only in the outer margins once the
        // opaque VIRGO panel is added above this layer.
        for (int i = 0; i < 70; ++i) {
            const float x = ((i * 193 + 71) % 997) / 997.f * w;
            const float y = ((i * 127 + 43) % 991) / 991.f * h;
            const float radius = (i % 13 == 0) ? 1.5f : ((i % 4 == 0) ? .95f : .58f);
            auto starNode = CCDrawNode::create();
            if (!starNode) continue;
            const auto tint = (i % 5 == 0) ? ccc4f(.56f, .78f, 1.f, .80f)
                                           : ccc4f(.76f, .84f, 1.f, .64f);
            starNode->drawDot({0, 0}, radius, tint);
            if (i % 13 == 0) {
                starNode->drawSegment({-radius * 2.8f, 0}, {radius * 2.8f, 0}, .45f,
                                      ccc4f(tint.r, tint.g, tint.b, .45f));
                starNode->drawSegment({0, -radius * 2.8f}, {0, radius * 2.8f}, .45f,
                                      ccc4f(tint.r, tint.g, tint.b, .45f));
            }
            starNode->setPosition({x, y});
            addChild(starNode, -10);
            starNode->runAction(CCRepeatForever::create(CCSequence::create(
                CCDelayTime::create((i % 11) * .13f),
                CCEaseSineInOut::create(CCScaleTo::create(1.2f + (i % 3) * .25f, 1.25f)),
                CCEaseSineInOut::create(CCScaleTo::create(1.2f + (i % 3) * .25f, .78f)), nullptr)));
        }

        // Put the Virgo constellation in the visible outer sky margin, never behind UI.
        drawVirgoConstellation(this, {w * .94f, h * .67f}, .18f, .68f, false);
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

        // A fully opaque, continuous content surface. This prevents the external
        // starfield from bleeding through and keeps every card readable.
        auto field = CCNode::create();
        m_panel->addChild(field, 3);
        const float fieldX = NAV_W + 9.f;
        const float fieldW = PANEL_W - fieldX - 8.f;
        const float fieldY = 8.f;
        const float fieldH = PANEL_H - 16.f;
        rect(field, fieldX, fieldY, fieldW, fieldH, rgba(8, 13, 30, 255), 0);

        // Very subtle header pinpoints only; the main content and cards remain clean.
        for (int i = 0; i < 14; ++i) {
            auto point = CCDrawNode::create();
            if (!point) continue;
            point->drawDot({0, 0}, .55f + (i % 3) * .18f,
                           ccc4f(.62f, .76f, 1.f, .27f));
            point->setPosition({fieldX + 18.f + static_cast<float>((i * 67 + 23) % 470),
                                330.f + static_cast<float>((i * 17) % 45)});
            field->addChild(point, 2);
            point->runAction(CCRepeatForever::create(CCSequence::create(
                CCEaseSineInOut::create(CCScaleTo::create(1.7f, 1.35f)),
                CCEaseSineInOut::create(CCScaleTo::create(1.7f, .8f)), nullptr)));
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
            const float y = PANEL_H - 112.f - i * 38.f;
            button(m_panel, TAB_NAMES[i], 91, y, 156, 30, i, false, false, i == m_selected ? 3 : 0);
        }
        button(m_panel, "GUIDE", 91, 33, 156, 34, 110);
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
        virgo::loadBrainData();
        virgo::loadSettings();
        clearPageHits();
        if (m_content) {
            m_content->removeFromParentAndCleanup(true);
            m_content = nullptr;
        }
        m_content = CCNode::create();
        if (!m_content) return;
        m_panel->addChild(m_content, 6);
        sectionTitle(m_selected == 5 ? "BRAIN / AUTOPLAY" : TAB_NAMES[m_selected], TAB_INFO[m_selected]);

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

            text(m_content, "LIVE CONSOLE", 294, 164, 176, 17, ccc3(208, 195, 255));
            text(m_content, "Open the log console when needed.",
                 294, 135, 176, 24, ccc3(136, 156, 205), "bigFont.fnt", .40f);
            button(m_content, s_consoleVisible ? "CLOSE CONSOLE" : "OPEN CONSOLE",
                   555, 143, 164, 32, 103, true);
        } else if (m_selected == 2) {
            text(m_content, "CHOOSE WHICH GAMEMODES USE THE PREDICTIVE BUFFER", 430, 326, 430, 17,
                 ccc3(145, 175, 226), "bigFont.fnt", .37f);

            const int ids[] = {0, 3, 2, 4, 1, 5, 6, 7};
            const char* names[] = {"CUBE", "BALL", "SHIP", "UFO", "WAVE", "ROBOT", "SPIDER", "SWING"};
            const char* descriptions[] = {"Standard jump", "Gravity flip", "Continuous flight", "Tap-to-float",
                                          "Wave trajectory", "Release jump", "Dash movement", "Gravity swing"};
            const ccColor4F colors[] = {
                ccc4f(.42f, .70f, 1.f, 1.f), ccc4f(.84f, .65f, 1.f, 1.f),
                ccc4f(.37f, .86f, .94f, 1.f), ccc4f(.94f, .72f, .48f, 1.f),
                ccc4f(.69f, .55f, 1.f, 1.f), ccc4f(.51f, .83f, .75f, 1.f),
                ccc4f(.95f, .58f, .79f, 1.f), ccc4f(.57f, .75f, 1.f, 1.f)
            };
            for (int i = 0; i < 8; ++i) {
                const int mode = ids[i];
                const int col = i % 2;
                const int row = i / 2;
                const float x = col == 0 ? 314.f : 550.f;
                const float y = 294.f - row * 74.f;
                const bool enabled = virgo::modeEnabled[mode];
                card(m_content, x - 109.f, y - 26.f, 218.f, 52.f,
                     enabled ? rgba(19, 31, 58, 255) : rgba(13, 18, 38, 255));
                drawGamemodeIcon(m_content, mode, {x - 82.f, y}, colors[i]);
                text(m_content, names[i], x - 43.f, y + 7.f, 104.f, 15.f,
                     enabled ? ccc3(232, 238, 255) : ccc3(135, 146, 174),
                     "bigFont.fnt", .52f);
                text(m_content, descriptions[i], x - 43.f, y - 11.f, 112.f, 11.f,
                     ccc3(123, 151, 198), "bigFont.fnt", .34f);
                button(m_content, enabled ? "ON" : "OFF", x + 73.f, y, 54.f, 23.f,
                       300 + mode, true, false, enabled ? 3 : 2);
            }
            text(m_content, "DISABLED MODES PASS INPUT THROUGH WITHOUT BUFFERING", 430, 22, 430, 13,
                 ccc3(115, 145, 191), "bigFont.fnt", .34f);
        } else if (m_selected == 1) {
            text(m_content, "STAGE PROFILES", 430, 326, 430, 17,
                 ccc3(145, 175, 226), "bigFont.fnt", .40f);
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
                const float y = row == 0 ? 267.f : 194.f;
                card(m_content, x - 71.f, y - 30.f, 142.f, 60.f,
                     i == m_selectedStage ? rgba(52, 39, 90, 255) : fills[i]);
                text(m_content, names[i], x, y + 13.f, 128.f, 15.f,
                     i == m_selectedStage ? ccc3(238, 214, 255) : ccc3(184, 194, 233),
                     "bigFont.fnt", .48f);
                text(m_content, detail[i], x, y - 4.f, 120.f, 10.f,
                     ccc3(145, 166, 220), "bigFont.fnt", .32f);
                button(m_content, i == m_selectedStage ? "ACTIVE" : "CHOOSE", x, y - 20.f,
                       92.f, 18.f, 200 + i, true, false, i == m_selectedStage ? 3 : 0);
            }

            text(m_content, "MAIN MODE FRAME LIMITS", 430, 133, 430, 13,
                 ccc3(150, 178, 229), "bigFont.fnt", .38f);
            frameControl(m_content, "CUBE", 0, 270.f, 91.f);
            frameControl(m_content, "WAVE", 1, 430.f, 91.f);
            frameControl(m_content, "SHIP", 2, 590.f, 91.f);
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
            // Stage profiles set the starting Cube/Wave/Ship limits; Custom keeps edits.
            static const int stageFrames[5][3] = {
                {2, 2, 2}, {4, 4, 4}, {8, 8, 8}, {16, 8, 16}, {24, 24, 24}
            };
            if (m_selectedStage < 5) {
                for (int i = 0; i < 3; ++i) virgo::modeFrames[i] = stageFrames[m_selectedStage][i];
            }
            virgo::selectedStage = m_selectedStage;
            virgo::saveSettings();
        } else if (tag >= 220 && tag < 226) {
            const int mode = (tag - 220) / 2;
            const bool increase = ((tag - 220) % 2) == 1;
            const int step = increase ? 1 : -1;
            virgo::modeFrames[mode] = std::clamp(virgo::modeFrames[mode] + step, 0, 60);
            virgo::saveSettings();
            buildContent();
            return;
        } else if (tag >= 300 && tag < 308) {
            const int mode = tag - 300;
            virgo::modeEnabled[mode] = !virgo::modeEnabled[mode];
            virgo::saveSettings();
            buildContent();
            return;
        } else if (tag == 102) {
            m_animated = !m_animated;
            virgo::animationsEnabled = m_animated;
            virgo::saveSettings();
        } else if (tag == 101) {
            m_quiet = !m_quiet;
            virgo::quietMode = m_quiet;
            virgo::saveSettings();
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
        virgo::loadSettings();
        m_selectedStage = virgo::selectedStage;
        m_quiet = virgo::quietMode;
        m_animated = virgo::animationsEnabled;
        virgo::loadSettings();
        m_selectedStage = virgo::selectedStage;
        m_quiet = virgo::quietMode;
        m_animated = virgo::animationsEnabled;
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


