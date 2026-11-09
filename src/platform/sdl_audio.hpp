#pragma once
// Plays the Asylum mixer through SDL: replaces the Sound Blaster driver of
// ASYLUM.DLL (DMA buffer refilled from the IRQ handler and ASSProcess).
// The device pulls mono 16-bit samples at the mixer's rate from its own
// thread.

#include "engine/audio/asylum_mixer.hpp"

#include <cstdint>

namespace crusader {

class SdlAudio {
public:
    // Opens the default output; ok() is false without a usable device.
    explicit SdlAudio(AsylumMixer& mixer);
    ~SdlAudio();
    SdlAudio(const SdlAudio&) = delete;
    SdlAudio& operator=(const SdlAudio&) = delete;

    bool ok() const { return device_ != 0; }

private:
    static void callback(void* self, std::uint8_t* stream, int len);

    AsylumMixer& mixer_;
    std::uint32_t device_ = 0;
};

}  // namespace crusader
