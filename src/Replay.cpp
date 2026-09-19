#include "Replay.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace target {
namespace {
constexpr std::size_t maxBytes = 32 * 1024 * 1024;
constexpr std::size_t maxInputs = 1000000;
constexpr std::uint64_t maxFrame = 240 * 60 * 60 * 24; // 24-hour replay limit.
void require(bool condition, char const* error) {
    if (!condition) throw std::runtime_error(error);
}

// GDR2's implementation uses length-prefixed UTF-8 strings and big-endian
// IEEE floats (the upstream README's older field table is not accurate).
class Reader {
    std::span<const std::uint8_t> data;
    std::size_t pos = 0;
public:
    explicit Reader(std::span<const std::uint8_t> bytes) : data(bytes) {}
    std::size_t remaining() const { return data.size() - pos; }
    std::uint8_t byte() {
        require(remaining() != 0, "Truncated GDR2 file.");
        return data[pos++];
    }
    bool boolean() {
        auto v = byte();
        require(v <= 1, "Invalid boolean in GDR2 file.");
        return v != 0;
    }
    std::uint64_t integer() {
        std::uint64_t value = 0;
        for (unsigned shift = 0; shift < 64; shift += 7) {
            auto v = byte();
            require(shift != 63 || v <= 1, "GDR2 integer overflow.");
            value |= std::uint64_t(v & 127) << shift;
            if (!(v & 128)) return value;
        }
        throw std::runtime_error("Invalid GDR2 integer.");
    }
    void skip(std::uint64_t n) {
        require(n <= remaining(), "Truncated GDR2 extension.");
        pos += static_cast<std::size_t>(n);
    }
    std::string string() {
        auto n = integer();
        require(n <= remaining() && n <= 65535, "Invalid GDR2 string.");
        std::string result(reinterpret_cast<char const*>(data.data() + pos), n);
        skip(n);
        return result;
    }
    template<class T, class Bits> T floating() {
        Bits bits = 0;
        for (std::size_t i = 0; i < sizeof(Bits); ++i) bits = (bits << 8) | byte();
        return std::bit_cast<T>(bits);
    }
};

Replay parseBinary(std::span<const std::uint8_t> bytes) {
    Reader in(bytes.subspan(3));
    require(in.integer() == 2, "Only GDR2 version 2 is supported.");
    Replay r;
    auto tag = in.string();
    in.string(); // Author
    in.string(); // Description
    r.duration = in.floating<float, std::uint32_t>();
    in.integer(); // Game version is metadata, not a compatibility guarantee.
    r.rate = in.floating<double, std::uint64_t>();
    r.seed = in.integer();
    in.integer(); // Coins
    r.ldm = in.boolean();
    require(!in.boolean(), "Platformer replays are not supported.");
    r.bot = in.string();
    in.integer(); // Bot version
    r.levelId = in.integer();
    r.levelName = in.string();
    auto extension = in.integer();
    r.hasCorrections = extension != 0 || !tag.empty();
    in.skip(extension);
    require(in.integer() == 0, "Replay contains recorded deaths. Use a continuous run.");
    auto count = in.integer();
    auto p1Count = in.integer();
    require(count > 0 && count <= maxInputs && count <= in.remaining(), "Invalid GDR2 input count.");
    require(p1Count <= count, "Invalid GDR2 player count.");
    r.inputs.reserve(count);
    std::uint64_t frame = 0;
    for (std::uint64_t i = 0; i < count; ++i) {
        if (i == p1Count) frame = 0;
        auto packed = in.integer();
        auto delta = packed >> 1;
        require(delta <= maxFrame && frame <= maxFrame - delta, "Replay exceeds 24 hours.");
        frame += delta;
        r.inputs.push_back({frame, 1, i >= p1Count, bool(packed & 1)});
        if (!tag.empty()) in.skip(in.integer());
    }
    // GJBaseGameLayer::handleButton's third argument uses the historical
    // Geometry Dash convention used by xdBot: false is player 1. xdBot's
    // GDR2 exporter stores the standardized player2 flag, so normalize it
    // before playback. GDR1 xdBot already follows the historical convention.
    if (r.bot == "xdBot") {
        for (auto& input : r.inputs) input.player2 = !input.player2;
    }
    require(in.remaining() == 0, "Unexpected trailing GDR2 data.");
    return r;
}

std::uint64_t unsignedField(nlohmann::json const& j) {
    require(j.is_number_integer(), "Expected a non-negative integer in replay.");
    require(!j.is_number_integer() || j.is_number_unsigned() || j.get<std::int64_t>() >= 0,
            "Negative frame, seed, or level ID.");
    return j.get<std::uint64_t>();
}

Replay parseLegacy(std::span<const std::uint8_t> bytes) {
    using Json = nlohmann::json;
    // Bound nesting before constructing a DOM from untrusted downloaded macros.
    struct DepthSax : nlohmann::json_sax<Json> {
        int depth = 0;
        bool null() override { return true; }
        bool boolean(bool) override { return true; }
        bool number_integer(number_integer_t) override { return true; }
        bool number_unsigned(number_unsigned_t) override { return true; }
        bool number_float(number_float_t, string_t const&) override { return true; }
        bool string(string_t&) override { return true; }
        bool binary(binary_t&) override { return true; }
        bool start_object(std::size_t) override { return ++depth <= 32; }
        bool key(string_t&) override { return true; }
        bool end_object() override { --depth; return true; }
        bool start_array(std::size_t) override { return ++depth <= 32; }
        bool end_array() override { --depth; return true; }
        bool parse_error(std::size_t, std::string const&, nlohmann::detail::exception const&) override { return false; }
    } sax;
    auto first = std::find_if(bytes.begin(), bytes.end(), [](auto c) { return c != ' ' && c != '\r' && c != '\n' && c != '\t'; });
    bool text = first != bytes.end() && *first == '{';
    auto format = text ? Json::input_format_t::json : Json::input_format_t::msgpack;
    require(Json::sax_parse(bytes.begin(), bytes.end(), &sax, format), "Invalid or excessively nested GDR JSON/MessagePack.");
    auto j = text ? Json::parse(bytes.begin(), bytes.end()) : Json::from_msgpack(bytes.begin(), bytes.end());
    Replay r;
    require(j.is_object(), "Expected a GDR replay object.");
    require(j.value("version", 1.0) == 1.0, "Unsupported legacy GDR version.");
    r.rate = j.value("framerate", 240.0);
    r.duration = j.value("duration", 0.0);
    r.seed = j.contains("seed") ? unsignedField(j.at("seed")) : 0;
    r.ldm = j.value("ldm", false);
    require(!j.value("platformer", false), "Platformer replays are not supported.");
    r.levelId = unsignedField(j.at("level").at("id"));
    r.levelName = j.at("level").value("name", std::string{});
    r.bot = j.at("bot").at("name").get<std::string>();
    r.legacyXdBot = r.bot == "xdBot";
    r.hasCorrections = j.contains("frameFixes") && !j.at("frameFixes").empty();
    require(!j.contains("deaths") || j.at("deaths").empty(), "Replay contains recorded deaths. Use a continuous run.");
    auto const& inputs = j.at("inputs");
    require(inputs.is_array() && !inputs.empty() && inputs.size() <= maxInputs, "Replay is empty or has too many inputs.");
    for (auto const& input : inputs) {
        auto frame = unsignedField(input.at("frame"));
        require(frame <= maxFrame, "Replay exceeds 24 hours.");
        auto button = unsignedField(input.at("btn"));
        require(button == 1, "Only Classic jump inputs are supported.");
        auto p2 = input.at("2p").get<bool>();
        r.inputs.push_back({frame, 1, p2, input.at("down").get<bool>()});
    }
    return r;
}
}

Replay parseReplay(std::span<const std::uint8_t> bytes) {
    require(!bytes.empty() && bytes.size() <= maxBytes, "Replay must be between 1 byte and 32 MiB.");
    auto r = bytes.size() >= 3 && bytes[0] == 'G' && bytes[1] == 'D' && bytes[2] == 'R'
        ? parseBinary(bytes) : parseLegacy(bytes);
    require(std::isfinite(r.rate) && std::abs(r.rate - 240.0) < 0.00001,
            "Only 240 TPS replays are supported. Export a 240 TPS macro.");
    require(std::isfinite(r.duration) && r.duration >= 0 && r.duration <= 86400, "Invalid replay duration.");
    require(r.levelId > 0 && r.levelId <= std::numeric_limits<std::uint32_t>::max(), "Replay must include a valid level ID.");
    std::stable_sort(r.inputs.begin(), r.inputs.end(), [](auto const& a, auto const& b) { return a.frame < b.frame; });
    return r;
}

Replay loadReplay(std::filesystem::path const& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    require(bool(file), "Cannot open replay file.");
    auto size = file.tellg();
    require(size > 0 && size <= static_cast<std::streamoff>(maxBytes), "Replay must be between 1 byte and 32 MiB.");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), size);
    require(bool(file), "Could not read the complete replay file.");
    return parseReplay(bytes);
}

int parseTarget(std::string const& text) {
    int value = 0;
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    require(error == std::errc{} && end == text.data() + text.size() && value >= 1 && value <= 100,
            "Enter a whole-number target from 1 to 100.");
    return value;
}

void Playback::start(int target) {
    require(target >= 1 && target <= 100, "Target must be 1 to 100.");
    goal = target;
    next = 0;
    state = RunState::Playing;
}
void Playback::cancel() { state = RunState::Cancelled; }
void Playback::observe(double percent, bool dead, bool completed) {
    if (state != RunState::Playing) return;
    if (completed) state = RunState::Reached;
    else if (dead) state = RunState::Failed;
    else if (std::isfinite(percent) && goal < 100 && percent >= goal) state = RunState::Reached;
}
}
