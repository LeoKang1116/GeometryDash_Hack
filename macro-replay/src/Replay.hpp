#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace target {
struct Input {
    std::uint64_t frame;
    int button;
    bool player2;
    bool down;
};

struct Replay {
    std::uint64_t levelId = 0;
    std::string levelName;
    std::string bot;
    double rate = 240;
    double duration = 0;
    std::uint64_t seed = 0;
    bool ldm = false;
    bool legacyXdBot = false;
    bool hasCorrections = false;
    std::vector<Input> inputs;
};

// Throws std::runtime_error with a user-readable explanation.
Replay parseReplay(std::span<const std::uint8_t> bytes);
Replay loadReplay(std::filesystem::path const& path);
int parseTarget(std::string const& text);

// GD 2.2081 counts two progress units per 240 Hz physics tick.
constexpr std::int64_t replayFrame(std::uint32_t progress, int offset) {
    return static_cast<std::int64_t>(progress / 2) - offset;
}

enum class RunState { Idle, Playing, Reached, Failed, Cancelled };
class Playback {
public:
    void start(int target);
    void cancel();
    void observe(double percent, bool dead, bool completed);
    template<class Emit>
    void dispatch(Replay const& replay, std::uint64_t frame, Emit emit) {
        if (state != RunState::Playing) return;
        while (next < replay.inputs.size() && replay.inputs[next].frame <= frame)
            emit(replay.inputs[next++]);
    }
    RunState state = RunState::Idle;
    int goal = 100;
    std::size_t next = 0;
};
}
