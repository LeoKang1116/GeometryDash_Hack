#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace autoclear {
using Frame = std::uint32_t;
struct Input { Frame frame; bool down; };
enum class State { Idle, Searching, Manual, Completed, Stopped, Exhausted };
int parseTarget(std::string const& text);

// Bounded best-first search over real-game attempts. No simulated physics.
class Solver {
public:
    void start(int target, unsigned limit = 500);
    void grounded(Frame frame);
    void died(Frame frame, double percent);
    bool retry();
    void observe(double percent, bool dead, bool completed);
    void takeOver();
    void stop();
    std::size_t inputsSent() const { return next; }
    template<class Emit> void dispatch(Frame frame, Emit emit) {
        if (state != State::Searching || pending) return;
        while (next < inputs.size() && inputs[next].frame <= frame)
            emit(inputs[next++]);
    }
    State state = State::Idle;
    int target = 50;
    unsigned attempts = 0;
    double bestPercent = 0;
    std::vector<Input> inputs;
private:
    struct Candidate { std::vector<Input> inputs; double score; };
    std::vector<Candidate> frontier;
    std::vector<Frame> ground;
    std::size_t next = 0;
    unsigned limit = 500;
    bool pending = false;
};
}
