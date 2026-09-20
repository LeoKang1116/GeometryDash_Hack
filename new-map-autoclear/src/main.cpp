#include "Solver.hpp"
#include "../../common/AutomationOwner.hpp"
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/utils/file.hpp>
#include <chrono>
#include <cmath>

using namespace geode::prelude;
namespace {
constexpr char ownerID[] = "leokang.new_map_autoclear";
constexpr char hudID[] = "leokang.new_map_autoclear/hud";
using Clock = std::chrono::steady_clock;
struct Session {
    autoclear::Solver solver;
    PlayLayer* owner = nullptr;
    bool injecting = false;
    bool arming = false;
    bool deathPending = false;
    bool released = true;
    autoclear::Frame deathFrame = 0;
    double deathPercent = 0;
    Clock::time_point began;
    std::string message;
} session;

autoclear::Frame frame(PlayLayer* pl) { return pl->m_gameState.m_currentProgress / 2; }
bool searching() { return session.solver.state == autoclear::State::Searching; }
void releaseInputs(PlayLayer* pl) {
    session.injecting = true;
    pl->handleButton(false, 1, false);
    pl->handleButton(false, 1, true);
    if (pl->m_player1) pl->m_player1->releaseAllButtons();
    if (pl->m_player2) pl->m_player2->releaseAllButtons();
    session.injecting = false;
}
void releaseControl(PlayLayer* pl) {
    if (session.released) return;
    session.released = true;
    session.deathPending = false;
    releaseInputs(pl);
    automation::release(pl, ownerID);
}
void hud(PlayLayer* pl) {
    auto label = static_cast<CCLabelBMFont*>(pl->getChildByID(hudID));
    if (!label) {
        label = CCLabelBMFont::create("", "chatFont.fnt");
        label->setID(hudID);
        auto size = CCDirector::sharedDirector()->getWinSize();
        label->setPosition({size.width / 2, size.height - 38});
        pl->addChild(label, 1000);
    }
    auto text = searching() ? fmt::format("AUTO {}% | Try {}/500 | Best {:.1f}%",
        session.solver.target, session.solver.attempts, session.solver.bestPercent) : session.message;
    label->setString(text.c_str());
    label->limitLabelWidth(420, .42f, .22f);
}
void stop(PlayLayer* pl, std::string message) {
    session.solver.stop();
    session.message = std::move(message);
    releaseControl(pl);
    hud(pl);
}
void saveResult(PlayLayer* pl) {
    // This is a solver trace, not GDR: local level ID 0 is valid here.
    matjson::Value events = matjson::Value::array();
    for (std::size_t n = 0; n < session.solver.inputsSent(); ++n) {
        auto const& input = session.solver.inputs[n];
        events.push(matjson::makeObject({{"frame", input.frame}, {"down", input.down}}));
    }
    auto result = matjson::makeObject({
        {"format", "new-map-autoclear-trace-v1"},
        {"game", "2.2081"}, {"tps", 240},
        {"level_id", pl->m_level->m_levelID.value()},
        {"level_data", std::string(pl->m_level->m_levelString)},
        {"target", session.solver.target}, {"actual_percent", pl->getCurrentPercent()},
        {"handoff_frame", frame(pl)}, {"release_at_handoff", true},
        {"completed", pl->m_hasCompletedLevel}, {"inputs", events}
    });
    auto saved = file::writeStringSafe(Mod::get()->getSaveDir() / "last-result.json", result.dump());
    if (saved.isErr()) log::warn("Could not save solver trace: {}", saved.unwrapErr());
}
void checkGoal(PlayLayer* pl) {
    if (!searching()) return;
    session.solver.observe(pl->getCurrentPercent(), pl->m_player1->m_isDead, pl->m_hasCompletedLevel);
    if (session.solver.state == autoclear::State::Manual || session.solver.state == autoclear::State::Completed) {
        releaseControl(pl);
        session.message = session.solver.state == autoclear::State::Manual
            ? "YOUR TURN - Auto OFF" : "Completed - Auto OFF";
        log::info("{} at {:.2f}%, physics frame {}, attempt {}", session.message,
            pl->getCurrentPercent(), frame(pl), session.solver.attempts);
        saveResult(pl);
        hud(pl);
    }
}
void validate(PlayLayer* pl) {
    if (auto old = Loader::get()->getLoadedMod("leokang.target_replay"))
        if (old->getVersion() < VersionInfo(0, 1, 3))
            throw std::runtime_error("Update Target Replay to 0.1.3 or disable it, then restart the game.");
    auto settings = pl->m_levelSettings;
    if (settings->m_platformerMode || settings->m_startMode != 0 || settings->m_startMini ||
        settings->m_startDual || settings->m_twoPlayerMode || settings->m_startSpeed != Speed::Normal ||
        settings->m_mirrorMode || settings->m_rotateGameplay || pl->m_isPracticeMode || pl->m_startPosObject)
        throw std::runtime_error("Use a normal-speed, full-size single cube level, without Practice or Start Position.");
    if (!pl->m_objects || pl->m_objects->count() > 2000)
        throw std::runtime_error("Use a short test level with at most 2000 objects.");
    // Deliberately narrow: no hidden triggers, portals, rotated/scaled geometry.
    for (auto object : CCArrayExt<GameObject*>(pl->m_objects)) {
        if (object->m_objectID != 1 && object->m_objectID != 8)
            throw std::runtime_error(fmt::format("Prototype supports basic block (1) and spike (8) only. Found object {}.", object->m_objectID));
        if (std::abs(object->getRotation()) > .01f ||
            std::abs(object->getScaleX() - 1.f) > .01f || std::abs(object->getScaleY() - 1.f) > .01f)
            throw std::runtime_error("Use unrotated, normal-size blocks and spikes.");
        auto rect = object->getObjectRect();
        log::info("Object {} at ({:.1f}, {:.1f}), bounds {:.1f} x {:.1f}",
            object->m_objectID, rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
    }
}
void restart(PlayLayer* pl) {
    session.arming = true;
    releaseInputs(pl);
    GameToolbox::fast_srand(1);
    pl->resetLevelFromStart();
    session.arming = false;
    session.deathPending = false;
    log::info("Solver attempt {} with {} inputs", session.solver.attempts, session.solver.inputs.size());
}
class AutoPopup : public Popup {
    TextInput* m_target = nullptr;
    WeakRef<PauseLayer> m_pause;
    void button(char const* text, SEL_MenuHandler handler, float x) {
        auto sprite = ButtonSprite::create(text);
        sprite->setScale(.6f);
        auto item = CCMenuItemSpriteExtra::create(sprite, this, handler);
        item->setPosition({x, 42}); m_buttonMenu->addChild(item);
    }
    void onStart(CCObject*) {
        auto pl = PlayLayer::get(); auto pause = m_pause.lock();
        if (!pl || !pause) return;
        try {
            auto goal = autoclear::parseTarget(m_target->getString());
            validate(pl);
            if (!automation::acquire(pl, ownerID))
                throw std::runtime_error("Another auto mode is running. Stop Target Replay first.");
            session.solver.start(goal);
            session.owner = pl; session.released = false;
            session.began = Clock::now(); session.message.clear();
            Mod::get()->setSettingValue<int64_t>("target-percent", goal);
            onClose(nullptr); pause->onResume(nullptr);
            restart(pl); hud(pl);
        } catch (std::exception const& e) {
            FLAlertLayer::create("Cannot start", e.what(), "OK")->show();
        }
    }
    void onManual(CCObject*) {
        auto pl = PlayLayer::get(); auto pause = m_pause.lock();
        if (!pl || !pause) return;
        if (session.owner == pl && searching()) {
            session.solver.takeOver(); releaseControl(pl);
            session.message = "YOUR TURN - Auto OFF"; hud(pl);
        }
        onClose(nullptr); pause->onResume(nullptr);
    }
public:
    static AutoPopup* create(PauseLayer* pause) {
        auto p = new AutoPopup;
        if (p->setup(pause)) { p->autorelease(); return p; }
        delete p; return nullptr;
    }
    bool setup(PauseLayer* pause) {
        if (!Popup::init(370, 240)) return false;
        m_pause = pause; setTitle("New Map Autoclear");
        auto info = CCLabelBMFont::create("Experimental: basic blocks + spikes only\nAuto retries to find jumps (500 tries / 5 min).\nAt target: YOUR TURN, without restarting.\n100% waits for level completion.", "chatFont.fnt");
        info->setScale(.42f); info->setPosition({185, 167}); m_mainLayer->addChild(info);
        m_target = TextInput::create(130, "1 - 100");
        m_target->setCommonFilter(CommonFilter::Uint); m_target->setMaxCharCount(3);
        m_target->setString(std::to_string(Mod::get()->getSettingValue<int64_t>("target-percent")));
        m_target->setLabel("Manual control at %"); m_target->setPosition({185, 95});
        m_mainLayer->addChild(m_target);
        button("Start", menu_selector(AutoPopup::onStart), 100);
        button("Take Over", menu_selector(AutoPopup::onManual), 265);
        return true;
    }
};
}
class $modify(AutoclearPause, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        auto menu = CCMenu::create();
        auto size = CCDirector::sharedDirector()->getWinSize();
        menu->setPosition({size.width - 75, size.height - 66});
        auto sprite = ButtonSprite::create("New Map Auto"); sprite->setScale(.45f);
        menu->addChild(CCMenuItemSpriteExtra::create(sprite, this, menu_selector(AutoclearPause::onAuto)));
        addChild(menu, 100);
    }
    void onAuto(CCObject*) { if (auto p = AutoPopup::create(this)) p->show(); }
};
class $modify(AutoclearInput, GJBaseGameLayer) {
    void processQueuedButtons(float dt, bool clearInputQueue) {
        auto pl = PlayLayer::get();
        if (pl && this == static_cast<GJBaseGameLayer*>(pl) && session.owner == pl && searching() &&
            !session.arming && !session.deathPending && !pl->m_player1->m_isDead) {
            checkGoal(pl);
            if (searching()) {
                auto p = pl->m_player1;
                if (p->m_isShip || p->m_isBird || p->m_isBall || p->m_isDart || p->m_isRobot ||
                    p->m_isSpider || p->m_isSwing || p->m_isUpsideDown || pl->m_gameState.m_isDualMode) {
                    stop(pl, "Unsupported mode - Auto OFF");
                } else {
                    if (p->m_isOnGround) session.solver.grounded(frame(pl));
                    session.injecting = true;
                    session.solver.dispatch(frame(pl), [this](autoclear::Input input) {
                        GJBaseGameLayer::handleButton(input.down, 1, GameManager::get()->getGameVariable("0010"));
                    });
                    session.injecting = false;
                }
            }
        }
        GJBaseGameLayer::processQueuedButtons(dt, clearInputQueue);
    }
    void handleButton(bool down, int button, bool player) {
        if (this == static_cast<GJBaseGameLayer*>(session.owner) && searching() && !session.injecting) return;
        GJBaseGameLayer::handleButton(down, button, player);
    }
};
class $modify(AutoclearPlay, PlayLayer) {
    void resetLevel() {
        if (session.owner == this && !session.arming) stop(this, "Restarted - Auto OFF");
        PlayLayer::resetLevel();
    }
    void destroyPlayer(PlayerObject* player, GameObject* object) {
        PlayLayer::destroyPlayer(player, object);
        if (session.owner == this && searching() && !session.arming && player->m_isDead && !session.deathPending) {
            session.deathFrame = frame(this); session.deathPercent = getCurrentPercent();
            session.deathPending = true;
            log::info("Solver death at frame {}, {:.2f}%", session.deathFrame, session.deathPercent);
        }
    }
    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if (session.owner != this || !searching() || session.arming || m_isPaused) return;
        if (Clock::now() - session.began > std::chrono::minutes(5)) {
            stop(this, "Time limit reached - Auto OFF"); pauseGame(false); return;
        }
        if (session.deathPending) {
            session.solver.died(session.deathFrame, session.deathPercent);
            session.deathPending = false;
            if (session.solver.retry()) restart(this);
            else { stop(this, "Search exhausted - Auto OFF"); pauseGame(false); }
        } else checkGoal(this);
        hud(this);
    }
    void levelComplete() {
        PlayLayer::levelComplete();
        if (session.owner == this) checkGoal(this);
    }
    void onExit() {
        if (session.owner == this) {
            session.solver.stop(); releaseControl(this); session.owner = nullptr;
        }
        PlayLayer::onExit();
    }
};
