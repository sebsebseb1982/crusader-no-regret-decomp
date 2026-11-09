#include "platform/sdl_audio.hpp"

#include <SDL.h>

namespace crusader {

SdlAudio::SdlAudio(AsylumMixer& mixer) : mixer_(mixer) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return;
    SDL_AudioSpec want{};
    want.freq = AsylumMixer::kMixRate;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 512;  // about 23 ms
    want.callback = &SdlAudio::callback;
    want.userdata = this;
    device_ = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);
    if (device_ != 0) SDL_PauseAudioDevice(device_, 0);
}

SdlAudio::~SdlAudio() {
    if (device_ != 0) SDL_CloseAudioDevice(device_);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void SdlAudio::callback(void* self, std::uint8_t* stream, int len) {
    auto* audio = static_cast<SdlAudio*>(self);
    audio->mixer_.mix(reinterpret_cast<std::int16_t*>(stream), static_cast<std::size_t>(len) / 2);
}

}  // namespace crusader
