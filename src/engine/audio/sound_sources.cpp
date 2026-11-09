#include "engine/audio/sound_sources.hpp"

#include <algorithm>
#include <cstdlib>

namespace crusader {

namespace {

constexpr int kFirstLoop = SoundSources::kSources - SoundSources::kLoopSources;
constexpr ObjId kPlayer = 1;  // DS:7E0E

}  // namespace

SoundSources::SoundSources(SoundManager& voices) : voices_(voices) {
    voices_.set_on_steal([this](SoundManager::Handle h) { release(h); });
}

// @orig 1450:0000
int SoundSources::volume_for(const World& world, ObjId item) {
    const Item* it = item ? world.get(item) : nullptr;
    if (!it || world.is_partly_on_screen(*it)) return 0x40;
    const Item* player = world.get(kPlayer);
    if (!player) return 0x40;
    const int d = std::max(std::abs(it->x - player->x), std::abs(it->y - player->y));
    if (d > 0x3C0) return 8;
    if (d > 0x280) return 0x10;
    if (d > 0x17C) return 0x20;
    return 0x32;
}

// @orig 1450:01D0
void SoundSources::refresh() {
    voices_.update();
    for (Source& s : sources_) {
        if (s.handle != SoundManager::kNoHandle && voices_.sfx_of(s.handle) == -1) s.handle = SoundManager::kNoHandle;
    }
}

// @orig 1450:07B9
void SoundSources::release(SoundManager::Handle handle) {
    for (Source& s : sources_) {
        if (s.handle == handle) {
            s.handle = SoundManager::kNoHandle;
            s.item = 0;
            return;
        }
    }
}

// @orig 1450:021E
void SoundSources::start(Source& s, const World& world, int sfx, ObjId item, bool loop) {
    const int volume = volume_for(world, item);
    s.item = item;
    s.volume = volume;
    s.initial_volume = volume;
    s.sfx = sfx;
    s.loop = loop;
    s.handle = voices_.play(sfx, volume, 0, loop);
}

// @orig 1450:021E
void SoundSources::play(const World& world, int sfx, ObjId item) {
    refresh();
    auto free = std::find_if(sources_.begin(), sources_.begin() + kFirstLoop,
                             [](const Source& s) { return s.handle == SoundManager::kNoHandle; });
    if (free != sources_.begin() + kFirstLoop && valid_sfx(sfx)) start(*free, world, sfx, item, false);
}

// @orig 1450:02C5
void SoundSources::play_ambient(const World& world, int sfx, ObjId item) {
    refresh();
    const auto sample = voices_.sample(sfx);
    if (!sample) return;
    if (sample->loop_length == 0) {
        play(world, sfx, item);
        return;
    }
    if (is_playing_anywhere(sfx)) return;
    auto free = std::find_if(sources_.begin() + kFirstLoop, sources_.end(),
                             [](const Source& s) { return s.handle == SoundManager::kNoHandle; });
    if (free != sources_.end() && valid_sfx(sfx)) start(*free, world, sfx, item, true);
}

// @orig 1450:03AD
void SoundSources::retrigger(const World& world, int sfx, ObjId item) {
    if (!valid_sfx(sfx)) return;
    for (int i = 0; i < kFirstLoop; ++i) {
        Source& s = sources_[static_cast<std::size_t>(i)];
        if (s.item != item || s.sfx != sfx) continue;
        // A stale handle plays the sound on a new voice whose handle the
        // source does not keep (as in the original).
        if (voices_.retrigger(s.handle, sfx, s.volume, s.initial_volume) != SoundManager::kNoHandle) return;
        s.item = 0;
        s.handle = SoundManager::kNoHandle;
    }
    play(world, sfx, item);
}

// @orig 1450:0486
void SoundSources::stop(int sfx, ObjId item) {
    if (!valid_sfx(sfx)) return;
    for (Source& s : sources_) {
        if (s.sfx == sfx && s.item == item) {
            voices_.stop(s.handle);
            s.handle = SoundManager::kNoHandle;
            s.item = 0;
            return;
        }
    }
}

// @orig 1450:0551
void SoundSources::stop_item(ObjId item) {
    for (Source& s : sources_) {
        if (s.item != item) continue;
        voices_.stop(s.handle);
        s.handle = SoundManager::kNoHandle;
        s.item = 0;
    }
}

// @orig 1450:05B1
void SoundSources::stop_item_loops(ObjId item) {
    for (Source& s : sources_) {
        if (s.item != item || !s.loop) continue;
        voices_.stop(s.handle);
        s.handle = SoundManager::kNoHandle;
        s.loop = false;
        s.item = 0;
    }
}

void SoundSources::detach(ObjId item) {
    for (Source& s : sources_) {
        if (s.item == item) s.item = 0;
    }
}

// @orig 1450:0633
bool SoundSources::is_playing(int sfx, ObjId item) {
    if (!valid_sfx(sfx)) return false;
    refresh();
    return std::any_of(sources_.begin(), sources_.end(), [&](const Source& s) {
        return s.item == item && s.sfx == sfx && voices_.sfx_of(s.handle) == sfx;
    });
}

// @orig 1450:06C0
bool SoundSources::is_playing_anywhere(int sfx) {
    if (!valid_sfx(sfx)) return false;
    refresh();
    return std::any_of(sources_.begin(), sources_.end(), [&](const Source& s) {
        return s.item != 0 && s.sfx == sfx && voices_.sfx_of(s.handle) == sfx;
    });
}

// @orig 1450:0750
void SoundSources::set_volume(int sfx, ObjId item, int volume) {
    if (!valid_sfx(sfx)) return;
    for (const Source& s : sources_) {
        if (s.sfx == sfx && s.item == item) {
            voices_.set_volume(s.handle, volume);
            return;
        }
    }
}

// @orig 1450:0813
void SoundSources::update_volumes(const World& world) {
    for (int i = kFirstLoop; i < kSources; ++i) {
        Source& s = sources_[static_cast<std::size_t>(i)];
        if (s.handle == SoundManager::kNoHandle || s.item == 0) continue;
        const int volume = volume_for(world, s.item);
        if (volume == s.volume) continue;
        s.volume = volume;
        voices_.set_volume(s.handle, volume);
    }
}

void SoundSources::clear() { sources_.fill(Source{}); }

}  // namespace crusader
