#include "Solver.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <stdexcept>

namespace autoclear {
int parseTarget(std::string const& text) {
    int value = 0;
    auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc{} || end != text.data() + text.size() || value < 1 || value > 100)
        throw std::runtime_error("Enter a whole number from 1 to 100.");
    return value;
}
void Solver::start(int goal, unsigned maxAttempts) {
    if (goal < 1 || goal > 100 || maxAttempts == 0)
        throw std::runtime_error("Invalid target or attempt limit.");
    *this = Solver{};
    target = goal;
    limit = maxAttempts;
    attempts = 1;
    state = State::Searching;
}
void Solver::grounded(Frame frame) {
    if (state != State::Searching || pending) return;
    if (ground.empty() || ground.back() < frame) ground.push_back(frame);
    // Keep about two-thirds of a second of possible jump takeoff times.
    std::erase_if(ground, [frame](Frame f) { return frame > f && frame - f > 160; });
}
void Solver::observe(double percent, bool dead, bool completed) {
    if (state != State::Searching || pending) return;
    if (std::isfinite(percent)) bestPercent = std::max(bestPercent, percent);
    if (completed) state = State::Completed;
    else if (!dead && target < 100 && std::isfinite(percent) && percent >= target)
        state = State::Manual;
}
void Solver::died(Frame frame, double percent) {
    if (state != State::Searching || pending) return;
    pending = true;
    double score = std::isfinite(percent) ? percent : 0;
    bestPercent = std::max(bestPercent, score);
    if (attempts >= limit) { state = State::Exhausted; return; }
    Frame last = inputs.empty() ? 0 : inputs.back().frame;
    // Preserve siblings, so an earlier jump can be changed when a later one fails.
    Frame previous = 0;
    bool sampled = false;
    if (inputs.size() < 128) for (auto f : ground) {
        if (f >= frame || frame - f > 160 || (!inputs.empty() && f <= last + 1)) continue;
        if (sampled && f - previous < 4) continue;
        previous = f;
        sampled = true;
        for (Frame hold : {1u, 12u, 36u}) {
            auto sequence = inputs;
            sequence.push_back({f, true});
            sequence.push_back({f + hold, false});
            frontier.push_back({std::move(sequence), score});
        }
    }
    std::stable_sort(frontier.begin(), frontier.end(), [](auto const& a, auto const& b) {
        return a.score > b.score;
    });
    if (frontier.size() > 1024) frontier.resize(1024);
    if (frontier.empty()) state = State::Exhausted;
}
bool Solver::retry() {
    if (state != State::Searching || !pending || frontier.empty()) return false;
    inputs = std::move(frontier.front().inputs);
    frontier.erase(frontier.begin());
    ground.clear(); next = 0; pending = false; ++attempts;
    return true;
}
void Solver::takeOver() {
    if (state == State::Searching) { state = State::Manual; pending = false; }
}
void Solver::stop() { state = State::Stopped; pending = false; }
}
