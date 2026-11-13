#include <Geode/Geode.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/GameManager.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/ui/BasedButtonSprite.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>

using namespace geode::prelude;

// ---------------------------------------------------------------------------
// Hack state (saved between sessions)
// ---------------------------------------------------------------------------

struct Hack {
    char const* key;
    char const* name;
    char const* desc;
    bool cheat; // turns on Safe Mode while used in a level
};

static std::vector<Hack> const HACKS = {
    { "noclip", "Noclip", "Can't die.", true },
    { "speedhack", "Speedhack", "Change the game speed (set below).", true },
    { "hitboxes", "Show Hitboxes", "Draw hitboxes while playing.", false },
    { "no-death-effect", "No Death Effect", "Skip the explosion when you die.", false },
    { "unlock-icons", "Unlock All Icons", "Use every icon and color in the garage.", false },
    { "overlay", "Info Overlay", "Show FPS, speed, noclip deaths and Safe Mode in levels.", false },
    { "instant-restart", "Instant Restart", "Restart right away after dying.", false },
};

static bool on(char const* key) {
    return Mod::get()->getSavedValue<bool>(fmt::format("hack-{}", key), false);
}
static void setOn(char const* key, bool value) {
    Mod::get()->setSavedValue<bool>(fmt::format("hack-{}", key), value);
}
static float speed() {
    return std::clamp(Mod::get()->getSavedValue<float>("speed", 1.f), 0.1f, 5.f);
}
static void setSpeed(float s) {
    Mod::get()->setSavedValue<float>("speed", std::round(std::clamp(s, 0.1f, 5.f) * 100.f) / 100.f);
}
static bool speedActive() {
    return on("speedhack") && std::abs(speed() - 1.f) > 0.001f;
}
static bool cheatsActive() {
    return on("noclip") || speedActive();
}

// ---------------------------------------------------------------------------
// The menu
// ---------------------------------------------------------------------------

class MgMenuPopup : public Popup {
protected:
    static constexpr float W = 360.f;
    static constexpr float H = 290.f;
    CCLabelBMFont* m_speedLbl = nullptr;
    CCLabelBMFont* m_safeLbl = nullptr;

    bool init() {
        if (!Popup::init(W, H)) return false;
        this->setTitle("MG Menu");

        float y = H - 52.f;
        float leftX = 34.f;
        for (auto const& hack : HACKS) {
            auto key = hack.key;
            auto toggle = CCMenuItemExt::createTogglerWithStandardSprites(0.6f, [this, key](CCMenuItemToggler* t) {
                // the toggler flips its state after this callback
                setOn(key, !t->isToggled());
                this->updateLabels();
            });
            toggle->toggle(on(key));
            toggle->setPosition({ leftX, y });
            m_buttonMenu->addChild(toggle);

            auto name = CCLabelBMFont::create(hack.name, "bigFont.fnt");
            name->setAnchorPoint({ 0.f, 0.5f });
            name->setScale(0.42f);
            name->setPosition({ leftX + 18.f, y + 4.f });
            if (hack.cheat) name->setColor({ 255, 160, 120 });
            m_mainLayer->addChild(name);

            auto desc = CCLabelBMFont::create(hack.desc, "chatFont.fnt");
            desc->setAnchorPoint({ 0.f, 0.5f });
            desc->limitLabelWidth(W - leftX - 40.f, 0.5f, 0.2f);
            desc->setColor({ 210, 210, 210 });
            desc->setPosition({ leftX + 18.f, y - 7.f });
            m_mainLayer->addChild(desc);
            y -= 27.f;
        }

        // Speed controls
        y -= 4.f;
        m_speedLbl = CCLabelBMFont::create("", "goldFont.fnt");
        m_speedLbl->setScale(0.6f);
        m_speedLbl->setPosition({ W / 2, y });
        m_mainLayer->addChild(m_speedLbl);

        auto addSpeedBtn = [this, y](char const* text, float x, std::function<float(float)> change) {
            auto spr = ButtonSprite::create(text, "bigFont.fnt", "GJ_button_04.png", 0.8f);
            spr->setScale(0.45f);
            auto btn = CCMenuItemExt::createSpriteExtra(spr, [this, change](CCMenuItemSpriteExtra*) {
                setSpeed(change(speed()));
                this->updateLabels();
            });
            btn->setPosition({ x, y });
            m_buttonMenu->addChild(btn);
        };
        addSpeedBtn("-", W / 2 - 80.f, [](float s) { return s - 0.1f; });
        addSpeedBtn("+", W / 2 + 80.f, [](float s) { return s + 0.1f; });
        float presetY = y - 24.f;
        float px = W / 2 - 96.f;
        for (float preset : { 0.25f, 0.5f, 0.75f, 1.f, 1.5f, 2.f }) {
            auto text = fmt::format("{}x", preset);
            auto spr = ButtonSprite::create(text.c_str(), "bigFont.fnt", "GJ_button_04.png", 0.8f);
            spr->setScale(0.38f);
            auto btn = CCMenuItemExt::createSpriteExtra(spr, [this, preset](CCMenuItemSpriteExtra*) {
                setSpeed(preset);
                this->updateLabels();
            });
            btn->setPosition({ px, presetY });
            m_buttonMenu->addChild(btn);
            px += 38.f;
        }

        m_safeLbl = CCLabelBMFont::create("", "chatFont.fnt");
        m_safeLbl->setScale(0.55f);
        m_safeLbl->setPosition({ W / 2, 14.f });
        m_mainLayer->addChild(m_safeLbl);

        this->updateLabels();
        return true;
    }

    void updateLabels() {
        m_speedLbl->setString(fmt::format("Speed: {:.2f}x{}", speed(), on("speedhack") ? "" : " (Speedhack off)").c_str());
        if (cheatsActive()) {
            m_safeLbl->setString("Safe Mode ON: completions and new bests won't save");
            m_safeLbl->setColor({ 255, 160, 120 });
        } else {
            m_safeLbl->setString("No cheats on: progress saves normally");
            m_safeLbl->setColor({ 160, 255, 160 });
        }
    }

public:
    static MgMenuPopup* create() {
        auto ret = new MgMenuPopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    static void open() {
        auto scene = CCDirector::sharedDirector()->getRunningScene();
        if (!scene || scene->getChildByID("mg-menu-popup"_spr)) return;
        if (auto popup = create()) {
            popup->setID("mg-menu-popup"_spr);
            popup->show();
        }
    }
};

static CCMenuItemSpriteExtra* menuButton(float scale) {
    auto spr = CircleButtonSprite::createWithSpriteFrameName("GJ_hammerIcon_001.png", 1.f, CircleBaseColor::Green, CircleBaseSize::Small);
    if (!spr) {
        auto b = ButtonSprite::create("MG", "bigFont.fnt", "GJ_button_01.png", 0.8f);
        b->setScale(scale);
        return CCMenuItemExt::createSpriteExtra(b, [](CCMenuItemSpriteExtra*) { MgMenuPopup::open(); });
    }
    spr->setScale(scale);
    auto label = CCLabelBMFont::create("MG", "bigFont.fnt");
    label->setScale(0.5f);
    label->setPosition({ spr->getContentSize().width / 2, -6.f });
    spr->addChild(label);
    return CCMenuItemExt::createSpriteExtra(spr, [](CCMenuItemSpriteExtra*) { MgMenuPopup::open(); });
}

// ---------------------------------------------------------------------------
// Hacks
// ---------------------------------------------------------------------------

// Speedhack: scale the time step of the whole game
class $modify(MgScheduler, CCScheduler) {
    void update(float dt) {
        CCScheduler::update(speedActive() ? dt * speed() : dt);
    }
};

class $modify(MgGameManager, GameManager) {
    bool isIconUnlocked(int id, IconType type) {
        if (on("unlock-icons")) return true;
        return GameManager::isIconUnlocked(id, type);
    }
    bool isColorUnlocked(int id, UnlockType type) {
        if (on("unlock-icons")) return true;
        return GameManager::isColorUnlocked(id, type);
    }
};

class $modify(MgPlayerObject, PlayerObject) {
    void playDeathEffect() {
        if (on("no-death-effect")) return;
        PlayerObject::playDeathEffect();
    }
};

class $modify(MgPlayLayer, PlayLayer) {
    struct Fields {
        bool usedCheats = false;   // sticks for this level visit
        int noclipDeaths = 0;
        bool noclipHitThisAttempt = false;
        CCLabelBMFont* overlay = nullptr;
        float fpsTimer = 0.f;
        int frames = 0;
        int fps = 0;
    };

    void markCheats() {
        if (cheatsActive() && !m_fields->usedCheats) {
            m_fields->usedCheats = true;
            log::info("[MG] Cheats used: Safe Mode on for this level");
        }
        // Test mode is what GD uses for start positions: nothing gets saved
        if (m_fields->usedCheats) m_isTestMode = true;
    }

    void setupOverlay() {
        auto label = CCLabelBMFont::create("", "bigFont.fnt");
        label->setAnchorPoint({ 0.f, 1.f });
        label->setScale(0.3f);
        label->setOpacity(190);
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        label->setPosition({ 4.f, winSize.height - 4.f });
        label->setID("mg-overlay"_spr);
        this->addChild(label, 10000);
        m_fields->overlay = label;
    }

#if GEODE_COMP_GD_VERSION >= 22091
    bool init(GJGameLevel* level, bool dontCreateObjects) {
        if (!PlayLayer::init(level, dontCreateObjects)) return false;
#else
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
#endif
        this->setupOverlay();
        this->markCheats();
        return true;
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (object != m_anticheatSpike && on("noclip")) {
            this->markCheats();
            if (!m_fields->noclipHitThisAttempt) {
                m_fields->noclipHitThisAttempt = true;
                m_fields->noclipDeaths++;
            }
            return;
        }
        PlayLayer::destroyPlayer(player, object);
        if (object != m_anticheatSpike && on("instant-restart") && player && player->m_isDead && !m_isPracticeMode) {
            // restart on the next frame, after GD finishes handling the death
            Loader::get()->queueInMainThread([self = Ref<PlayLayer>(this)] {
                if (self->getParent()) self->resetLevel();
            });
        }
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        m_fields->noclipHitThisAttempt = false;
    }

    void levelComplete() {
        this->markCheats();
        PlayLayer::levelComplete();
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        this->markCheats();

        // Hitboxes: use GD's built-in hitbox drawing
        if (on("hitboxes") != m_isDebugDrawEnabled) {
            m_isDebugDrawEnabled = on("hitboxes");
            if (!m_isDebugDrawEnabled && m_debugDrawNode) m_debugDrawNode->clear();
        }
        if (m_isDebugDrawEnabled) this->updateDebugDraw();

        auto f = m_fields.self();
        if (!f->overlay) return;
        f->overlay->setVisible(on("overlay"));
        if (!on("overlay")) return;

        // real (unscaled) frame time for FPS
        f->frames++;
        f->fpsTimer += CCDirector::sharedDirector()->getDeltaTime() / (speedActive() ? speed() : 1.f);
        if (f->fpsTimer >= 0.5f) {
            f->fps = static_cast<int>(std::round(f->frames / f->fpsTimer));
            f->frames = 0;
            f->fpsTimer = 0.f;
        }
        std::string text = fmt::format("FPS: {}", f->fps);
        if (speedActive()) text += fmt::format("\nSpeed: {:.2f}x", speed());
        if (on("noclip")) text += fmt::format("\nNoclip deaths: {}", f->noclipDeaths);
        if (f->usedCheats) text += "\nSAFE MODE";
        f->overlay->setString(text.c_str());
    }
};

// ---------------------------------------------------------------------------
// Ways to open the menu: pause menu, main menu, PC shortcut
// ---------------------------------------------------------------------------

class $modify(MgPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto menu = CCMenu::create();
        menu->setPosition({ 0.f, 0.f });
        menu->setID("mg-pause-menu"_spr);
        auto btn = menuButton(0.8f);
        btn->setPosition({ winSize.width - 36.f, 36.f });
        menu->addChild(btn);
        this->addChild(menu, 100);
    }
};

static enumKeyCodes openKey() {
    auto key = Mod::get()->getSettingValue<std::string>("open-key");
    if (key == "F1") return KEY_F1;
    if (key == "F2") return KEY_F2;
    if (key == "Insert") return KEY_Insert;
    if (key == "Right Shift") return KEY_RightShift;
    return KEY_Tab;
}

class $modify(MgKeyboard, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool repeat, double time) {
        if (down && !repeat && key == openKey()) {
            Loader::get()->queueInMainThread([] { MgMenuPopup::open(); });
            return true;
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat, time);
    }
};

// ---------------------------------------------------------------------------
// Self-updater (same as the other mods)
// ---------------------------------------------------------------------------

#if GEODE_COMP_GD_VERSION >= 22091
static constexpr auto RELEASE_BASE = "https://github.com/pester33/Mg/releases/download/latest-build-2.2091/";
#else
static constexpr auto RELEASE_BASE = "https://github.com/pester33/Mg/releases/download/latest-build/";
#endif

static async::TaskHolder<web::WebResponse> s_updateTask;
static bool s_updateBusy = false;

static void whenScreenReady(std::function<void()> fn, int tries = 0) {
    auto scene = CCDirector::sharedDirector()->getRunningScene();
    bool ready = scene && !typeinfo_cast<CCTransitionScene*>(scene);
    if (ready || tries > 600) {
        fn();
        return;
    }
    Loader::get()->queueInMainThread([fn = std::move(fn), tries]() mutable {
        whenScreenReady(std::move(fn), tries + 1);
    });
}

static void installUpdate(std::string version) {
    if (s_updateBusy) return;
    s_updateBusy = true;
    auto url = fmt::format("{}{}.geode", RELEASE_BASE, Mod::get()->getID());
    log::info("[updater] Downloading {} from {}", version, url);
    auto loading = Notification::create("Downloading MG Menu update...", NotificationIcon::Loading, 0.f);
    loading->show();

    web::WebRequest req;
    req.timeout(std::chrono::seconds(90));
    s_updateTask.spawn(req.get(url), [loading = Ref(loading), version](web::WebResponse res) {
        s_updateBusy = false;
        loading->hide();
        auto const& data = res.data();
        log::info("[updater] Download finished: HTTP {}, {} bytes", res.code(), data.size());
        if (!res.ok() || data.size() < 4 || data[0] != 'P' || data[1] != 'K') {
            Notification::create(fmt::format("Update failed to download (HTTP {})", res.code()), NotificationIcon::Error, 3.f)->show();
            return;
        }
        auto written = file::writeBinarySafe(Mod::get()->getPackagePath(), data);
        if (!written) {
            log::warn("[updater] Couldn't write the mod file: {}", written.unwrapErr());
            Notification::create("Update failed to install", NotificationIcon::Error, 3.f)->show();
            return;
        }
        whenScreenReady([version] {
            createQuickPopup(
                "Update installed",
                fmt::format("MG Menu <cg>{}</c> is installed.\nRestart GD to use it.", version),
                "Later", "Restart",
                [](auto, bool restart) { if (restart) game::restart(true); }
            );
        });
    });
}

static void checkForUpdate(bool isRetry = false) {
    if (s_updateBusy) return;
    s_updateBusy = true;
    auto url = fmt::format("{}version.txt", RELEASE_BASE);
    log::info("[updater] Checking {} (current {})", url, Mod::get()->getVersion().toVString());

    web::WebRequest req;
    req.timeout(std::chrono::seconds(20));
    s_updateTask.spawn(req.get(url), [isRetry](web::WebResponse res) {
        s_updateBusy = false;
        auto body = utils::string::trim(res.string().unwrapOr(""));
        log::info("[updater] version.txt: HTTP {}, body '{}'", res.code(), body.substr(0, 40));
        if (!res.ok()) {
            if (!isRetry) Loader::get()->queueInMainThread([] { checkForUpdate(true); });
            return;
        }
        auto latest = VersionInfo::parse(body);
        if (!latest) return;
        auto current = Mod::get()->getVersion();
        if (!(latest.unwrap() > current)) return;
        auto version = latest.unwrap().toVString();
        whenScreenReady([version, current] {
            createQuickPopup(
                "MG Menu",
                fmt::format("A new version is out: <cg>{}</c> (you have {}).\nUpdate now?", version, current.toVString()),
                "Later", "Update",
                [version](auto, bool update) { if (update) installUpdate(version); }
            );
        });
    });
}

class $modify(MgMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        static bool firstTime = true;
        if (firstTime) {
            firstTime = false;
            if (Mod::get()->getSettingValue<bool>("check-updates")) checkForUpdate();
        }
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto menu = CCMenu::create();
        menu->setPosition({ 0.f, 0.f });
        menu->setID("mg-main-menu"_spr);
        auto btn = menuButton(0.75f);
        btn->setPosition({ winSize.width - 34.f, winSize.height / 2.f + 30.f });
        menu->addChild(btn);
        this->addChild(menu, 10);
        return true;
    }
};
