#include "Replay.hpp"
#include <nlohmann/json.hpp>
#include <bit>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

using nlohmann::json;
void check(bool value, char const* description) {
    if (!value) throw std::runtime_error(description);
}
template<class F> void rejects(F fn, char const* description) {
    try { fn(); } catch (std::exception const&) { return; }
    throw std::runtime_error(description);
}
std::vector<std::uint8_t> text(json const& j) {
    auto s = j.dump();
    return {s.begin(), s.end()};
}
json fixture() {
    return {{"version",1.0},{"framerate",240},{"duration",10},
        {"bot",{{"name","Test"},{"version","1"}}},
        {"level",{{"id",123},{"name","Test Level"}}},
        {"inputs",json::array({{{"frame",12},{"btn",1},{"2p",false},{"down",false}},
                              {{"frame",0},{"btn",1},{"2p",false},{"down",true}},
                              {{"frame",0},{"btn",1},{"2p",true},{"down",true}}})}};
}
// Independent wire-format fixture, including separated P1 and P2 delta streams.
std::vector<std::uint8_t> binaryFixture() {
    std::vector<std::uint8_t> b{'G','D','R'};
    auto var = [&](std::uint64_t n) {
        do { auto v = n & 127; n >>= 7; b.push_back(v | (n ? 128 : 0)); } while(n);
    };
    auto str = [&](std::string const& s) { var(s.size()); b.insert(b.end(),s.begin(),s.end()); };
    auto fp = [&]<class T>(T value) {
        auto bytes = std::bit_cast<std::array<std::uint8_t,sizeof(T)>>(value);
        if constexpr (std::endian::native == std::endian::little) std::reverse(bytes.begin(),bytes.end());
        b.insert(b.end(),bytes.begin(),bytes.end());
    };
    var(2); str(""); str("Author"); str(""); fp(10.f); var(22081); fp(240.0);
    var(42); var(0); var(0); var(0); str("Test"); var(1); var(123); str("Test Level");
    var(0); var(0); var(3); var(2);
    var(1); var(24); var(7); // P1 down @0, up @12; P2 down @3.
    return b;
}

int main(int argc, char** argv) {
    try {
        auto r = target::parseReplay(text(fixture()));
        check(r.inputs.size() == 3 && r.inputs[0].frame == 0 && r.inputs[1].player2, "stable input sorting");
        auto packed = target::parseReplay(json::to_msgpack(fixture()));
        check(packed.levelId == r.levelId && packed.inputs.size() == 3, "MessagePack import");
        auto xd = fixture(); xd["bot"]["name"] = "xdBot";
        check(target::parseReplay(text(xd)).inputs[0].player2, "legacy xdBot player convention");
        auto bytes = binaryFixture();
        auto binary = target::parseReplay(bytes);
        check(binary.seed == 42 && binary.inputs.size() == 3 && binary.inputs[1].frame == 3 && binary.inputs[1].player2, "GDR2 player delta reset");
        for (std::size_t n = 0; n < bytes.size(); ++n)
            rejects([&] { target::parseReplay(std::span(bytes).first(n)); }, "truncated GDR2 accepted");
        auto garbage = bytes; garbage.push_back(0);
        rejects([&] { target::parseReplay(garbage); }, "trailing GDR2 bytes accepted");
        for (auto fps : {0.0, 60.0, 360.0, -240.0}) {
            auto j = fixture(); j["framerate"] = fps;
            rejects([&] { target::parseReplay(text(j)); }, "unsupported tick rate accepted");
        }
        for (auto key : {"inputs","level","bot"}) {
            auto j = fixture(); j.erase(key);
            rejects([&] { target::parseReplay(text(j)); }, "missing required metadata accepted");
        }
        auto invalid = fixture(); invalid["inputs"][0]["frame"] = -1;
        rejects([&] { target::parseReplay(text(invalid)); }, "negative frame accepted");
        invalid = fixture(); invalid["inputs"][0]["btn"] = 257;
        rejects([&] { target::parseReplay(text(invalid)); }, "invalid button accepted");
        invalid = fixture(); invalid["platformer"] = true;
        rejects([&] { target::parseReplay(text(invalid)); }, "platformer accepted");
        for (std::string s : {"", "0", "101", "73.5", "73x", " 73", "-1"})
            rejects([&] { target::parseTarget(s); }, "invalid target accepted");
        check(target::parseTarget("73") == 73, "target parsing");
        target::Playback run;
        run.start(73);
        int events = 0;
        run.dispatch(r,0,[&](auto) { ++events; });
        run.dispatch(r,0,[&](auto) { ++events; });
        check(events == 2, "duplicate dispatch at same physics frame");
        run.observe(72.99,false,false);
        check(run.state == target::RunState::Playing,"stopped before target");
        run.observe(73.01,false,false);
        check(run.state == target::RunState::Reached,"target crossing not detected");
        run.dispatch(r,20,[&](auto) { ++events; });
        check(events == 2,"inputs emitted after stopping");
        run.start(100); run.observe(100,false,false);
        check(run.state == target::RunState::Playing,"100% did not wait for completion");
        run.observe(100,false,true);
        check(run.state == target::RunState::Reached,"completion not detected");
        run.start(73); run.observe(73,true,false);
        check(run.state == target::RunState::Failed,"natural death incorrectly claimed success");
        run.start(73); run.observe(std::numeric_limits<double>::quiet_NaN(),false,false);
        check(run.state == target::RunState::Playing,"NaN marked reached");
        run.cancel(); run.dispatch(r,100,[&](auto) { ++events; });
        check(events == 2,"cancelled inputs emitted");
        for (int i = 1; i < argc; ++i) {
            auto actual = target::loadReplay(argv[i]);
            std::cout << "Imported " << actual.levelName << ": " << actual.inputs.size() << " inputs\n";
        }
        std::cout << "All replay parser and playback tests passed.\n";
        return EXIT_SUCCESS;
    } catch (std::exception const& e) {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
