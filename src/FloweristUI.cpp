// Flowerist Rewrite - Phase 2 GUI
// All UI changes happen on Geometry Dash's Cocos main thread.
#include <Geode/Geode.hpp>
#include <Geode/modify/CCDirector.hpp>
#include <Windows.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <string>

using namespace geode::prelude;
namespace {
constexpr float PANEL_W = 516.f;
constexpr float PANEL_H = 310.f;
constexpr float NAV_W = 155.f;
constexpr int TAB_COUNT = 6;
const char* TAB_NAMES[TAB_COUNT] = {"LOG", "MODES", "STAGES", "ASSISTS", "SETTINGS", "COMPETITIVE MODE"};
const char* TAB_INFO[TAB_COUNT] = {
    "Recent activity and input diagnostics", "Choose your buffer profile", "Stages 1 - 4, Extreme and Custom",
    "Gameplay assistance overview", "Visuals and performance preferences", "Competition-focused profile (manual)"
};
ccColor4B color(int r,int g,int b,int a=255){return ccc4(r,g,b,a);}

CCLayerColor* rect(CCNode* parent, float x, float y, float w, float h, ccColor4B c, int z=0) {
    auto n = CCLayerColor::create(c,w,h);
    n->setPosition({x,y});
    parent->addChild(n,z);
    return n;
}
CCLabelBMFont* label(CCNode* parent,const std::string& txt,float x,float y,float maxW,float height,
                     ccColor3B tint=ccc3(120,61,91),bool shadow=true,const char* font="bigFont.fnt") {
    auto make = [&](const std::string& t) {return CCLabelBMFont::create(t.c_str(),font);};
    if(shadow) {
        auto sh=make(txt);
        if(sh) {
            float bw=std::max(1.f,sh->getContentSize().width), bh=std::max(1.f,sh->getContentSize().height);
            sh->setScale(std::min({0.68f,maxW/bw,height/bh}));
            sh->setColor(ccc3(101,47,72)); sh->setOpacity(82);
            sh->setPosition({x+1.f,y-1.1f}); parent->addChild(sh,1);
        }
    }
    auto l=make(txt);
    if (!l) return nullptr;
    float bw=std::max(1.f,l->getContentSize().width), bh=std::max(1.f,l->getContentSize().height);
    l->setScale(std::min({0.68f,maxW/bw,height/bh}));
    l->setColor(tint);l->setPosition({x,y});parent->addChild(l,2);
    return l;
}

// Drawn directly: no external assets or font files needed.
CCDrawNode* makeFlower(CCNode* parent,CCPoint at,float radius,int z=4) {
    auto d=CCDrawNode::create();
    for(int j=0;j<5;j++) {
        float angle=static_cast<float>(j)*6.2831853f/5.f;
        d->drawDot({std::cos(angle)*radius*0.68f,std::sin(angle)*radius*0.68f},radius*0.55f,ccc4f(1.f,.78f,.89f,.72f));
    }
    d->drawDot({0,0},radius*0.35f,ccc4f(1.f,.93f,.65f,1.f));
    d->setPosition(at);parent->addChild(d,z);return d;
}
}

class FloweristOverlay final : public CCLayer {
    CCNode* m_panel = nullptr;
    CCNode* m_content = nullptr;
    CCMenu* m_menu = nullptr;
    CCLayerColor* m_tabs[TAB_COUNT]{};
    CCLayerColor* m_glows[TAB_COUNT]{};
    CCMenuItemLabel* m_buttons[TAB_COUNT]{};
    CCLabelBMFont* m_lokText=nullptr;
    CCDrawNode* m_lokFace=nullptr;
    int m_selected=0;
    int m_hovered=-1;
    float m_elapsed=0.f;
    bool m_closing=false;
    bool m_quiet=false;
    bool m_animated=true;
    static FloweristOverlay* s_active;

    void speech(const std::string& text) {
        if(m_lokText) m_lokText->setString(text.c_str());
        if(m_lokFace) {
            m_lokFace->stopAllActions();
            if(m_animated) m_lokFace->runAction(CCRepeatForever::create(CCSequence::create(
                CCEaseSineInOut::create(CCMoveBy::create(.4f,{0,2.f})),
                CCEaseSineInOut::create(CCMoveBy::create(.4f,{0,-2.f})),nullptr)));
        }
    }
    void onTab(CCObject* obj) {
        auto item=static_cast<CCNode*>(obj);
        int chosen=item->getTag();
        if(chosen<0||chosen>=TAB_COUNT||chosen==m_selected) return;
        m_selected=chosen;
        updateNav();
        buildContent();
        speech(std::string("LOK: Welcome to ")+TAB_NAMES[chosen]+"!");
    }
    void onButton(CCObject* obj) {
        const int action=static_cast<CCNode*>(obj)->getTag();
        if(action==100) {
            close();return;
        }
        if(action==101) {
            m_quiet=!m_quiet;
            speech(m_quiet?"LOK: Quiet mode enabled!":"LOK: Sounds good! Quiet mode off.");
        } else if(action==102) {
            m_animated=!m_animated;
            speech(m_animated?"LOK: Animation is on!":"LOK: Reduced animations enabled.");
        } else if(action>=200 && action<206) {
            speech(std::string("LOK: ")+std::to_string(action-199)+" selected for preview!");
        } else if(action==300) {
            speech("LOK: Tuning will learn from future attempts!");
        } else {
            speech("LOK: This feature is planned for Phase 3!");
        }
        buildContent();
    }
    CCMenuItemLabel* addButton(CCNode* target,const std::string& txt,float x,float y,float width,int tag,
                                SEL_MenuHandler cb,bool nav=false) {
        auto caption=CCLabelBMFont::create(txt.c_str(),"bigFont.fnt");
        if(!caption)return nullptr;
        caption->setColor(ccc3(116,59,92));
        caption->setScale(std::min(.55f,width/std::max(1.f,caption->getContentSize().width)));
        auto btn=CCMenuItemLabel::create(caption,this,cb);
        if(!btn)return nullptr;
        if (nav) {
            btn->setContentSize({136.f,26.f});
            caption->setPosition({68.f,13.f});
        }
        btn->setTag(tag);
        btn->setPosition({x,y});
        if(nav) m_menu->addChild(btn,8); else target->addChild(btn,8);
        return btn;
    }
    void updateNav() {
        for(int i=0;i<TAB_COUNT;i++) {
            if(m_tabs[i]) m_tabs[i]->setColor(i==m_selected?ccc3(247,192,216):ccc3(255,230,241));
            if(m_glows[i])m_glows[i]->setOpacity(i==m_selected?175:0);
        }
    }
    void buildContent() {
        if(!m_panel)return;
        if(m_content) {m_content->removeFromParentAndCleanup(true);m_content=nullptr;}
        m_content=CCNode::create();m_panel->addChild(m_content,6);
        const float cx=337.f;
        label(m_content,TAB_NAMES[m_selected],cx,259.f,287.f,25.f,ccc3(156,65,107),true,"goldFont.fnt");
        label(m_content,TAB_INFO[m_selected],cx,236.f,288.f,14.f,ccc3(158,91,125));
        rect(m_content,175,68,322,147,color(255,235,244,230),1);
        rect(m_content,179,72,314,139,color(255,245,250,240),2);
        auto section=[&](const char* title,const char* detail,float y){
            label(m_content,title,cx,y,280,16,ccc3(167,72,116));
            label(m_content,detail,cx,y-20,285,13,ccc3(143,99,121));
        };
        if(m_selected==0) {
            section("LIVE LOGS","Console output throttled to avoid stutter",180);
            section("DIAGNOSTICS","Press F6 in-level to cycle tick presets",125);
        } else if(m_selected==1) {
            section("BUFFER ENGINE","60 tick hard cap • optimized trajectory scan",180);
            section("PROFILES","Balanced / Low End / Custom (planned)",125);
        } else if(m_selected==2) {
            section("STAGE PRESETS","1 | 2 | 3 | 4 | EXTREME | CUSTOM",180);
            section("AUTO TUNING","Learning from attempts is planned",125);
        } else if(m_selected==3) {
            section("ASSISTS","Input buffering enabled in the engine",180);
            section("COMING NEXT","Individual assists and detailed controls",125);
        } else if(m_selected==4) {
            section("ANIMATIONS",m_animated?"Enabled - softly animated visuals":"Reduced - lower visual overhead",180);
            section("QUIET LOGS",m_quiet?"Enabled for GUI status":"Off - standard GUI status",125);
            auto mm=CCMenu::create(); mm->setPosition({0,0});m_content->addChild(mm,7);
            addButton(mm,"TOGGLE ANIMATIONS",cx-78,87,140,102,menu_selector(FloweristOverlay::onButton));
            addButton(mm,"QUIET UI",cx+92,87,100,101,menu_selector(FloweristOverlay::onButton));
        } else {
            section("COMPETITIVE MODE","Manual preset selection / fair-play notice",180);
            section("NOTICE","Confirm your event rules before using assists",125);
        }
        // Low-cost tab transition: one ease, no per-frame rebuilding.
        m_content->setPosition({9.f,0.f}); // Slide-in, no opacity cascades.
        if(m_animated)m_content->runAction(CCEaseSineOut::create(CCMoveTo::create(.20f,{0,0})));
        else m_content->setPosition({0,0});
    }
    void decorate() {
        rect(m_panel,0,0,PANEL_W,PANEL_H,color(231,145,182,255),0);
        rect(m_panel,3,3,PANEL_W-6,PANEL_H-6,color(255,228,239,255),1);
        rect(m_panel,7,7,PANEL_W-14,PANEL_H-14,color(255,241,247,255),2);
        rect(m_panel,7,7,NAV_W,PANEL_H-14,color(251,211,229,255),3);
        rect(m_panel,NAV_W+7,7,2,PANEL_H-14,color(228,156,189),4);
        rect(m_panel,9,PANEL_H-52,NAV_W-4,43,color(250,197,220),4);
        label(m_panel,"FLOWERIST",85,PANEL_H-30,145,23,ccc3(165,53,107),true,"goldFont.fnt");
        label(m_panel,"BLOOM ENGINE",85,PANEL_H-49,139,12,ccc3(154,88,120));
        for(int i=0;i<12;i++) {
            const float x=172.f+(i*73%320);
            const float y=17.f+(i*47%272);
            auto flower=makeFlower(m_panel,{x,y},2.5f+(i%3),3);
            if(m_animated) flower->runAction(CCRepeatForever::create(CCSequence::create(
                CCMoveBy::create(1.4f+(i%4)*.3f,{0,7.f}),
                CCMoveBy::create(1.4f+(i%4)*.3f,{0,-7.f}),nullptr)));
        }
        m_menu=CCMenu::create();m_menu->setPosition({0,0});m_panel->addChild(m_menu,10);
        for(int i=0;i<TAB_COUNT;i++) {
            const float y=PANEL_H-85.f-i*36.f;
            m_glows[i]=rect(m_panel,15,y-14,142,28,color(255,122,178,0),4);
            m_tabs[i]=rect(m_panel,18,y-12,136,24,color(255,230,241),5);
            m_buttons[i]=addButton(m_panel,TAB_NAMES[i],86,y,126,i,
                                    menu_selector(FloweristOverlay::onTab),true);
        }
        updateNav();
        // LOK speech bubble + oval, flower-topped avatar drawn procedurally.
        rect(m_panel,183,14,306,42,color(220,149,181),6);
        rect(m_panel,185,16,302,38,color(255,253,254),7);
        auto bubble=CCDrawNode::create();
        bubble->drawSegment({200,16},{195,9},2.f,ccc4f(1.f,.99f,1.f,1.f));
        m_panel->addChild(bubble,7);
        m_lokText=label(m_panel,"LOK: Hello! I will guide you.",350,36,257,28,
                        ccc3(127,68,103),false);
        auto lok=CCNode::create();lok->setPosition({190,38});m_panel->addChild(lok,10);
        m_lokFace=CCDrawNode::create();
        m_lokFace->drawDot({0,0},17,ccc4f(1.f,.77f,.88f,1.f));
        m_lokFace->drawDot({-6,3},2.2f,ccc4f(.4f,.19f,.3f,1.f));
        m_lokFace->drawDot({6,3},2.2f,ccc4f(.4f,.19f,.3f,1.f));
        m_lokFace->drawSegment({-3,-5},{3,-5},1.3f,ccc4f(.57f,.32f,.43f,1.f));
        m_lokFace->setScaleX(1.22f);lok->addChild(m_lokFace,2);
        makeFlower(lok,{4,17},7.f,3);
        label(m_panel,"F2  -  OPEN / CLOSE",78,15,132,10,ccc3(151,91,120),false);
        auto exitMenu=CCMenu::create();exitMenu->setPosition({0,0});m_panel->addChild(exitMenu,12);
        addButton(exitMenu,"X",PANEL_W-22,PANEL_H-21,14,100,menu_selector(FloweristOverlay::onButton));
    }

public:
    static FloweristOverlay* active(){return s_active;}
    static FloweristOverlay* create() {
        auto p=new FloweristOverlay();
        if(p&&p->init()){p->autorelease();return p;}
        delete p;return nullptr;
    }
    bool init() override {
        if(!CCLayer::init())return false;
        auto win=CCDirector::sharedDirector()->getWinSize();
        auto dim=CCLayerColor::create(color(47,23,39,150),win.width,win.height);
        addChild(dim,0);
        m_panel=CCNode::create();
        m_panel->setContentSize({PANEL_W,PANEL_H});
        m_panel->setAnchorPoint({.5f,.5f});
        m_panel->ignoreAnchorPointForPosition(false);
        m_panel->setPosition({win.width/2,win.height/2});
        const float fit=std::min({1.f,(win.width-16.f)/PANEL_W,(win.height-16.f)/PANEL_H});
        m_panel->setScale(fit*.82f);
        addChild(m_panel,1);
        decorate();buildContent();
        if(m_animated) m_panel->runAction(CCEaseBackOut::create(CCScaleTo::create(.34f,fit)));
        else m_panel->setScale(fit);
        s_active=this;
        scheduleUpdate();
        speech("LOK: Hello! Pick a tab to explore!");
        return true;
    }
    void update(float dt) override {
        m_elapsed+=dt;
        if(m_elapsed<.04f)return;
        m_elapsed=0;
        // Hover zones are fully contained in the sidebar, and fit the full tab label.
        HWND window=GetForegroundWindow();
        DWORD process=0;GetWindowThreadProcessId(window,&process);
        if(process!=GetCurrentProcessId())return;
        POINT pt{};if(!GetCursorPos(&pt)||!ScreenToClient(window,&pt))return;
        RECT rc{};GetClientRect(window,&rc);
        float cw=static_cast<float>(rc.right-rc.left),ch=static_cast<float>(rc.bottom-rc.top);
        if(cw<=0||ch<=0)return;
        auto win=CCDirector::sharedDirector()->getWinSize();
        CCPoint cursor{pt.x*win.width/cw,(ch-pt.y)*win.height/ch};
        CCPoint local=m_panel->convertToNodeSpace(cursor);
        int hover=-1;
        if(local.x>=16 && local.x<=155) {
            for(int i=0;i<TAB_COUNT;i++) {
                const float y=PANEL_H-85.f-i*36.f;
                if(local.y>=y-14 && local.y<=y+14){hover=i;break;}
            }
        }
        if(hover!=m_hovered) {
            m_hovered=hover;
            for(int i=0;i<TAB_COUNT;i++) {
                if(!m_glows[i])continue;
                const int dest=i==hover?230:(i==m_selected?175:0);
                m_glows[i]->stopAllActions();
                m_glows[i]->runAction(CCFadeTo::create(m_animated?.13f:0.f,dest));
            }
        }
    }
    void onExit() override {
        if(s_active==this) s_active=nullptr;
        CCLayer::onExit();
    }
    void close() {
        if(m_closing)return;
        m_closing=true;
        if(s_active==this)s_active=nullptr;
        unscheduleUpdate();
        // Closing animation does not keep the overlay alive indefinitely.
        if(m_panel&&m_animated) {
            m_panel->runAction(CCEaseSineIn::create(CCScaleTo::create(.14f,m_panel->getScale()*.88f)));
            runAction(CCSequence::create(CCDelayTime::create(.15f),CCCallFunc::create(this,callfunc_selector(FloweristOverlay::removeSelf)),nullptr));
        } else removeSelf();
    }
    void removeSelf(){removeFromParentAndCleanup(true);}
};
FloweristOverlay* FloweristOverlay::s_active=nullptr;

class $modify(FloweristGlobalFrame, CCDirector) {
    void drawScene() {
        // Edge-detect F2 to prevent repeatedly opening the menu while held.
        static bool lastDown=false;
        bool down=(GetAsyncKeyState(VK_F2)&0x8000)!=0;
        if(down&&!lastDown) {
            if(auto overlay=FloweristOverlay::active()) overlay->close();
            else if(auto scene=CCDirector::sharedDirector()->getRunningScene()) {
                if(auto overlay=FloweristOverlay::create())scene->addChild(overlay,999999);
            }
        }
        lastDown=down;
        CCDirector::drawScene();
    }
};
