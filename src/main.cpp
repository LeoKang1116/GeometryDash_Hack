#include "Replay.hpp"
#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/utils/file.hpp>
#include <cmath>
#include <optional>

using namespace geode::prelude;

namespace {
struct Session {
    std::optional<target::Replay> replay;
    target::Playback playback;
    PlayLayer* owner = nullptr; // Cleared in onExit; never used by asynchronous callbacks.
    bool injecting = false;
    bool arming = false;
    bool finishPending = false;
    int frameOffset = 0;
    std::string filename;
    std::string result;
} session;

void releaseInputs(PlayLayer* pl) {
    session.injecting = true;
    for (bool p1 : {true, false}) pl->handleButton(false, 1, p1);
    if (pl->m_player1) pl->m_player1->releaseAllButtons();
    if (pl->m_player2) pl->m_player2->releaseAllButtons();
    session.injecting = false;
}

void finish(PlayLayer* pl, bool kill) {
    releaseInputs(pl);
    session.result = fmt::format("{} at {:.2f}% (target {}%)",
        session.playback.state == target::RunState::Reached ? "Target reached" : "Replay failed",
        pl->getCurrentPercent(), session.playback.goal);
    session.finishPending = true;
    if (kill && pl->m_player1 && !pl->m_player1->m_isDead)
        pl->destroyPlayer(pl->m_player1, nullptr);
}

class TargetPopup : public Popup {
    TextInput* m_target = nullptr;
    TextInput* m_offset = nullptr;
    CCLabelBMFont* m_file = nullptr;
    WeakRef<PauseLayer> m_pause;

    void button(char const* text, SEL_MenuHandler handler, float x, float y) {
        auto sprite = ButtonSprite::create(text);
        sprite->setScale(0.65f);
        auto item = CCMenuItemSpriteExtra::create(sprite, this, handler);
        item->setPosition({x, y});
        m_buttonMenu->addChild(item);
    }
    void describe() {
        auto text = session.replay
            ? fmt::format("{} | Level {} | {} inputs{}", session.filename,
                session.replay->levelId, session.replay->inputs.size(),
                session.replay->hasCorrections ? "\nContains corrections: input-only playback may desync" : "")
            : "Choose a .gdr, .gdr.json or .gdr2 file";
        m_file->setString(text.c_str());
        m_file->limitLabelWidth(345.f, 0.35f, 0.17f);
    }
    void onBrowse(CCObject*) {
        async::spawn(file::pick(file::PickMode::OpenFile, file::FilePickOptions{
            .filters = {{"Geometry Dash replay", {"*.gdr", "*.gdr2", "*.json"}}}
        }), [self = Ref(this)](file::PickResult result) {
            if (!self->getParent()) return;
            if (result.isErr()) {
                FLAlertLayer::create("File picker", result.unwrapErr(), "OK")->show();
                return;
            }
            auto path = result.unwrap();
            if (!path) return;
            try {
                auto replay = target::loadReplay(*path);
                session.playback.cancel();
                session.finishPending = false;
                if (auto pl = PlayLayer::get()) releaseInputs(pl);
                session.replay = std::move(replay);
                session.filename = path->filename().string();
                self->describe();
            } catch (std::exception const& e) {
                FLAlertLayer::create("Cannot load replay", e.what(), "OK")->show();
            }
        });
    }
    void onStart(CCObject*) {
        auto pl = PlayLayer::get();
        auto pause = m_pause.lock();
        if (!pl || !pause) return;
        try {
            auto goal = target::parseTarget(m_target->getString());
            if (!session.replay) throw std::runtime_error("Choose a replay file first.");
            if (pl->m_levelSettings->m_platformerMode)
                throw std::runtime_error("Only Classic levels are supported.");
            if (pl->m_isPracticeMode || pl->m_startPosObject || pl->m_isTestMode)
                throw std::runtime_error("Use Normal mode from the beginning, without a Start Position.");
            if (static_cast<std::uint64_t>(pl->m_level->m_levelID.value()) != session.replay->levelId)
                throw std::runtime_error("This replay belongs to a different level ID.");
            int offset = 0;
            auto value = m_offset->getString();
            std::size_t used = 0;
            offset = std::stoi(value, &used);
            if (used != value.size() || offset < -10 || offset > 10)
                throw std::runtime_error("Frame offset must be an integer from -10 to 10.");
            Mod::get()->setSavedValue("frame-offset", offset);
            Mod::get()->setSettingValue<std::int64_t>("target-percent", goal);
            session.playback.cancel();
            releaseInputs(pl);
            session.owner = pl;
            session.frameOffset = offset;
            session.result.clear();
            session.finishPending = false;
            session.arming = true;
            onClose(nullptr);
            pause->onResume(nullptr);
            pl->resetLevelFromStart();
            session.arming = false;
            session.playback.start(goal);
        } catch (std::exception const& e) {
            FLAlertLayer::create("Cannot start", e.what(), "OK")->show();
        }
    }
    void onStop(CCObject*) {
        session.playback.cancel();
        session.finishPending = false;
        session.result = "Stopped by user";
        if (auto pl = PlayLayer::get()) releaseInputs(pl);
        onClose(nullptr);
    }
public:
    static TargetPopup* create(PauseLayer* pause) {
        auto popup = new TargetPopup;
        if (popup->setup(pause)) { popup->autorelease(); return popup; }
        delete popup;
        return nullptr;
    }
    bool setup(PauseLayer* pause) {
        if (!Popup::init(380.f, 270.f)) return false;
        m_pause = pause;
        setTitle("Target Replay");
        m_file = CCLabelBMFont::create("", "chatFont.fnt");
        m_file->setPosition({190.f, 210.f});
        m_mainLayer->addChild(m_file);
        describe();
        m_target = TextInput::create(115.f, "1 - 100");
        m_target->setCommonFilter(CommonFilter::Uint);
        m_target->setMaxCharCount(3);
        m_target->setString(std::to_string(Mod::get()->getSettingValue<std::int64_t>("target-percent")));
        m_target->setLabel("Target %");
        m_target->setPosition({115.f, 124.f});
        m_mainLayer->addChild(m_target);
        m_offset = TextInput::create(115.f, "0");
        m_offset->setCommonFilter(CommonFilter::Int);
        m_offset->setString(std::to_string(Mod::get()->getSavedValue<int>("frame-offset", 0)));
        m_offset->setLabel("Frame offset");
        m_offset->setPosition({265.f, 124.f});
        m_mainLayer->addChild(m_offset);
        auto info = CCLabelBMFont::create("Start restarts this level. 100% waits for completion.", "chatFont.fnt");
        info->setScale(0.4f);
        info->setPosition({190.f, 84.f});
        m_mainLayer->addChild(info);
        button("Choose macro", menu_selector(TargetPopup::onBrowse), 190.f, 173.f);
        button("Start", menu_selector(TargetPopup::onStart), 130.f, 46.f);
        button("Stop", menu_selector(TargetPopup::onStop), 250.f, 46.f);
        return true;
    }
};
}

class $modify(TargetPause, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        auto menu = CCMenu::create();
        auto size = CCDirector::sharedDirector()->getWinSize();
        menu->setPosition({size.width - 66.f, size.height - 28.f});
        auto sprite = ButtonSprite::create("Target Replay");
        sprite->setScale(0.5f);
        menu->addChild(CCMenuItemSpriteExtra::create(sprite, this, menu_selector(TargetPause::onTarget)));
        addChild(menu, 100);
        if (!session.result.empty()) {
            auto label = CCLabelBMFont::create(session.result.c_str(), "chatFont.fnt");
            label->limitLabelWidth(size.width - 30.f, 0.5f, 0.25f);
            label->setPosition({size.width / 2.f, size.height - 55.f});
            addChild(label, 100);
        }
    }
    void onTarget(CCObject*) {
        if (auto popup = TargetPopup::create(this)) popup->show();
    }
};

class $modify(TargetGameLayer, GJBaseGameLayer) {
    void processCommands(float dt, bool halfTick, bool lastTick) {
        GJBaseGameLayer::processCommands(dt, halfTick, lastTick);
        auto pl = PlayLayer::get();
        if (!pl || static_cast<GJBaseGameLayer*>(pl) != this || session.owner != pl || !session.replay ||
            session.playback.state != target::RunState::Playing ||
            pl->m_player1->m_isDead || pl->m_levelEndAnimationStarted) return;
        // Use the game's physics counter, never rendered frames. Old xdBot uses
        // levelTime * TPS + 1, matching its recorder's clock.
        auto frame = session.replay->legacyXdBot
            ? static_cast<std::int64_t>(pl->m_gameState.m_levelTime * 240.0) + 1
            : static_cast<std::int64_t>(pl->m_gameState.m_currentProgress);
        frame -= session.frameOffset;
        if (frame < 0) return;
        session.injecting = true;
        session.playback.dispatch(*session.replay, frame, [this](target::Input const& input) {
            bool p1 = !input.player2;
            if (GameManager::get()->getGameVariable("0010")) p1 = !p1;
            GJBaseGameLayer::handleButton(input.down, input.button, p1);
        });
        session.injecting = false;
    }
    void handleButton(bool down, int button, bool player1) {
        if (this == static_cast<GJBaseGameLayer*>(session.owner) && session.playback.state == target::RunState::Playing && !session.injecting)
            return; // Physical keys must not get mixed into the macro.
        GJBaseGameLayer::handleButton(down, button, player1);
    }
};

class $modify(TargetPlay, PlayLayer) {
    void resetLevel() {
        if (session.owner == this) {
            if (!session.arming) session.playback.cancel();
            releaseInputs(this);
            if (session.arming && session.replay) GameToolbox::fast_srand(session.replay->seed);
        }
        PlayLayer::resetLevel();
    }
    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        if (session.owner != this) return;
        if (session.playback.state == target::RunState::Playing) {
            session.playback.observe(getCurrentPercent(), m_player1->m_isDead, m_hasCompletedLevel);
            if (session.playback.state == target::RunState::Reached && !m_hasCompletedLevel)
                finish(this, true);
            else if (session.playback.state == target::RunState::Failed) finish(this, false);
        }
        if (session.finishPending && !m_isPaused && !m_hasCompletedLevel) {
            session.finishPending = false;
            pauseGame(false);
        }
    }
    void destroyPlayer(PlayerObject* player, GameObject* object) {
        PlayLayer::destroyPlayer(player, object);
        if (session.owner == this && player->m_isDead && session.playback.state == target::RunState::Playing) {
            session.playback.observe(getCurrentPercent(), true, false);
            finish(this, false);
        }
    }
    void levelComplete() {
        PlayLayer::levelComplete();
        if (session.owner == this && session.playback.state == target::RunState::Playing) {
            session.playback.observe(100, false, true);
            session.result = "Completed 100%";
            releaseInputs(this);
        }
    }
    void onExit() {
        if (session.owner == this) {
            releaseInputs(this);
            session.playback.cancel();
            session.owner = nullptr;
            session.finishPending = false;
            session.result.clear();
        }
        PlayLayer::onExit();
    }
};
