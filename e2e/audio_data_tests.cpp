// Music and samples checked against the original game data (data/, not in the
// repository): runs locally only.

#include "core/amf_module.hpp"
#include "core/sound_flex.hpp"
#include "engine/audio/audio.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace crusader;

namespace {

int failures = 0;

#define CHECK(cond)                                                    \
    do {                                                               \
        if (!(cond)) {                                                 \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                \
        }                                                              \
    } while (0)

void test_music(const std::filesystem::path& data) {
    const auto dir = data / "SOUND";
    if (!std::filesystem::exists(dir / "M01.AMF")) return;
    int modules = 0;
    for (const char* name : {"m01", "m02", "m03", "m04", "m05", "m06", "m07", "m08", "m09", "m10", "m11", "m12",
                             "m13", "m14", "m15", "m16a", "m16b", "m16c", "cred", "menu"}) {
        modules += AmfModule::load(dir / (std::string(name) + ".amf")).has_value();
    }
    CHECK(modules == 20);
    const auto m01 = AmfModule::load(dir / "M01.AMF");
    CHECK(m01 && m01->pattern_count == 83 && m01->order_count == 90 && m01->samples[0].pcm.size() == m01->samples[0].length);

    // Track names; 16 moves on before loading.
    AsylumMixer mixer;
    Music music(mixer, dir);
    CHECK(music.file_name(1) == "m01" && music.file_name(15) == "m15" && music.file_name(17) == "cred" &&
          music.file_name(18) == "menu" && !music.file_name(0) && !music.file_name(19));
    music.play(16);
    CHECK(music.current() == 16 && music.file_name(16) == "m16b" && mixer.music_playing());
    music.play(16);
    music.play(16);
    CHECK(music.file_name(16) == "m16a");
    // The menu over the game's track, then back.
    music.play(3);
    music.push(Music::kMenu);
    CHECK(music.current() == Music::kMenu);
    music.pop();
    CHECK(music.current() == 3 && music.flags() == 1);
    music.stop();
    CHECK(!mixer.music_playing() && music.flags() == 0);
    music.play(0);
    CHECK(music.current() == 0 && !mixer.music_playing());

    // Ten seconds of M01: sound, and the song moves on.
    music.play(1);
    std::vector<std::int16_t> out(22050 * 10);
    mixer.mix(out.data(), out.size());
    int loud = 0;
    for (std::int16_t s : out) loud += std::abs(s) > 256;
    std::printf("M01: %d of %zu samples above 256, order %d\n", loud, out.size(), mixer.music().order_position());
    CHECK(loud > 22050 && mixer.music().order_position() > 1);
}

void test_sound_flex(const std::filesystem::path& data) {
    const auto path = data / "SOUND/SOUND.FLX";
    if (!std::filesystem::exists(path)) return;
    const SoundFlex flex = SoundFlex::open(path);
    int sounds = 0;
    for (std::size_t n = 1; n < flex.count(); ++n) sounds += flex.sample(static_cast<int>(n)).has_value();
    std::printf("SOUND.FLX: %zu entries, %d sounds\n", flex.count(), sounds);
    CHECK(sounds == 269);
    const auto foot = flex.sample(1);
    CHECK(flex.name(1) == "FOOT1" && foot && foot->pcm.size() == 0x113 && foot->rate == 11025);
    const auto fan = flex.sample(6);
    CHECK(flex.name(6) == "FAN11" && fan && fan->loop_start == 0 && fan->loop_length == 0x1796);
    CHECK(flex.name(9) == "NC22" && flex.name(0xD0) == "INTRUDER" && flex.name(0x8F) == "DETHUM05");
}

}  // namespace

int main(int argc, char** argv) {
    const std::filesystem::path data = argc > 1 ? argv[1] : "data";
    if (!std::filesystem::exists(data / "SOUND/SOUND.FLX")) {
        std::printf("game data not found in %s, skipping\n", data.string().c_str());
        return 0;
    }
    test_sound_flex(data);
    test_music(data);
    if (failures) {
        std::fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    std::printf("audio data tests passed\n");
    return 0;
}
