#include "Solver.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace autoclear;
void check(bool value, char const* message) { if (!value) throw std::runtime_error(message); }
int main() {
    try {
        for (auto value : {"", "0", "101", "12.5", "12x", "-1"}) {
            bool rejected = false;
            try { parseTarget(value); } catch (...) { rejected = true; }
            check(rejected, "invalid percent accepted");
        }
        check(parseTarget("50") == 50, "target parse");
        Solver s; s.start(50);
        for (Frame f = 0; f < 100; ++f) s.grounded(f);
        s.died(100, 10); check(s.retry(), "no candidate after baseline death");
        int emitted = 0;
        s.dispatch(0, [&](auto) { ++emitted; });
        s.dispatch(0, [&](auto) { ++emitted; });
        check(emitted == 1, "duplicate input at same frame");
        s.observe(49.9, false, false); check(s.state == State::Searching, "early handoff");
        s.observe(50.1, false, false); check(s.state == State::Manual, "handoff missed");
        s.dispatch(1000, [&](auto) { ++emitted; });
        check(s.inputsSent() == 1, "trace contains unexecuted inputs after handoff");
        s.died(1000, 51); check(!s.retry() && emitted == 1, "automation restarted after handoff");
        s.start(50); s.observe(50, true, false);
        check(s.state == State::Searching, "death mistaken for handoff");
        s.start(100); s.observe(100, false, false);
        check(s.state == State::Searching, "100 did not wait for completion");
        s.observe(100, false, true); check(s.state == State::Completed, "completion missed");
        s.start(50); s.observe(std::numeric_limits<double>::quiet_NaN(), false, false);
        check(s.state == State::Searching, "NaN reached target");
        s.takeOver(); check(!s.retry() && s.state == State::Manual, "manual override failed");
        s.start(50, 1); s.grounded(1); s.died(20, 1);
        check(s.state == State::Exhausted && !s.retry(), "attempt budget ignored");
        s.start(50); s.grounded(10); s.grounded(14); s.died(20, 1);
        check(s.retry(), "first branch absent");
        auto first = s.inputs;
        s.died(20, 1); check(s.retry(), "alternative branch was lost");
        check(s.inputs.back().frame != first.back().frame, "did not backtrack to sibling");
        s.stop(); check(!s.retry(), "stop did not cancel retry");
        // A tiny independent oracle: death at 100 unless press at 40..44.
        s.start(50);
        while (s.state == State::Searching) {
            bool clears = false;
            for (Frame f = 0; f < 100; ++f) {
                s.grounded(f);
                s.dispatch(f, [&](Input i) { if (i.down && f >= 40 && f <= 44) clears = true; });
            }
            if (clears) s.observe(50, false, false);
            else { s.died(100, 10); s.retry(); }
        }
        check(s.state == State::Manual, "search failed simple independent oracle");
        std::cout << "Solver and handoff tests passed\n";
    } catch (std::exception const& e) { std::cerr << e.what() << '\n'; return 1; }
}
