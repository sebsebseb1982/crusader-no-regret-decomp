#include "engine/usecode/audio_intrinsics.hpp"

#include "engine/audio/audio.hpp"

#include <array>
#include <initializer_list>

namespace crusader {

namespace {

using Args = std::span<const std::uint8_t>;

std::uint16_t arg16(Args a, std::size_t pos) {
    return pos + 2 <= a.size() ? static_cast<std::uint16_t>(a[pos] | a[pos + 1] << 8) : 0;
}

// The object id behind the item pointer at the start of the arguments; the
// sound sources keep the number even when no item has it.
// @orig none: argument helper
ObjId item_id(UCMachine& m, Args a) {
    std::array<std::uint8_t, 2> id{};
    const std::uint32_t ptr = arg16(a, 0) | std::uint32_t{arg16(a, 2)} << 16;
    if (!m.dereference(ptr, id)) return 0;
    return static_cast<ObjId>(id[0] | id[1] << 8);
}

}  // namespace

// @orig none: binds the intrinsics, numbered by USECODE/UNKCOFF.DAT
std::vector<std::uint16_t> register_audio_intrinsics(UCMachine& m, World& w) {
    std::vector<std::uint16_t> bound;
    // (item, sfx) intrinsics: the call does nothing without sound.
    using ItemSound = void (*)(Audio&, const World&, int sfx, ObjId item);
    auto bind_item = [&](std::uint16_t n, ItemSound fn) {
        m.set_intrinsic(n, [&w, fn](UCMachine& machine, UCProcess&, Args a) -> std::uint32_t {
            if (Audio* audio = w.audio()) fn(*audio, w, arg16(a, 4), item_id(machine, a));
            return 0;
        });
        bound.push_back(n);
    };
    // AudioProcess::I_playSFXCru (10A8:3934).
    bind_item(0x015, [](Audio& au, const World& w, int sfx, ObjId item) { au.sources.play(w, sfx, item); });
    // AudioProcess::I_playAmbientSFXCru (10A8:3953).
    bind_item(0x02A, [](Audio& au, const World& w, int sfx, ObjId item) { au.sources.play_ambient(w, sfx, item); });
    // AudioProcess::I_playSFXCru (10A8:3991): restarts the item's sound.
    bind_item(0x045, [](Audio& au, const World& w, int sfx, ObjId item) { au.sources.retrigger(w, sfx, item); });
    // AudioProcess::I_stopSFXCru (10A8:39B0).
    bind_item(0x03B, [](Audio& au, const World&, int sfx, ObjId item) { au.sources.stop(sfx, item); });

    // AudioProcess::I_isSFXPlayingForObject (10A8:39EB).
    m.set_intrinsic(0x03D, [&w](UCMachine& machine, UCProcess&, Args a) -> std::uint32_t {
        Audio* audio = w.audio();
        return audio && audio->sources.is_playing(arg16(a, 4), item_id(machine, a));
    });
    // AudioProcess::I_stopSFXCru (10A8:39CF): every sound of the item.
    m.set_intrinsic(0x077, [&w](UCMachine& machine, UCProcess&, Args a) -> std::uint32_t {
        if (Audio* audio = w.audio()) audio->sources.stop_item(item_id(machine, a));
        return 0;
    });
    // AudioProcess::I_playSFX (12E0:0476).
    for (std::uint16_t n : {std::uint16_t{0x0AB}, std::uint16_t{0x0D4}}) {
        m.set_intrinsic(n, [&w](UCMachine&, UCProcess&, Args a) -> std::uint32_t {
            if (Audio* audio = w.audio()) audio->play_sfx(arg16(a, 0));
            return 0;
        });
        bound.push_back(n);
    }
    // AudioProcess::I_stopAllSFX (12E0:054A).
    m.set_intrinsic(0x0D1, [&w](UCMachine&, UCProcess&, Args) -> std::uint32_t {
        if (Audio* audio = w.audio()) audio->stop_all();
        return 0;
    });
    // MusicProcess::I_playMusic (1200:0143): the track number.
    m.set_intrinsic(0x012, [&w](UCMachine&, UCProcess&, Args a) -> std::uint32_t {
        if (Audio* audio = w.audio()) audio->music.play(static_cast<std::int16_t>(arg16(a, 0)));
        return 0;
    });
    // MusicProcess::I_stopMusic (1200:0223).
    m.set_intrinsic(0x091, [&w](UCMachine&, UCProcess&, Args) -> std::uint32_t {
        if (Audio* audio = w.audio()) audio->music.stop();
        return 0;
    });
    bound.insert(bound.end(), {0x03D, 0x077, 0x0D1, 0x012, 0x091});
    return bound;
}

}  // namespace crusader
