#pragma once
// The music half of the Asylum Sound System (ASYLUM.DLL): an 8-channel
// ProTracker-style player for AMF modules. The mixer calls tick() at the
// start of each of its buffers (1000:092A), one buffer per tick.
//
// Sequencer (1000:092A): a tick counter counts down from the speed; when it
// runs out, the next row is read (1000:062D, one cell per channel), and every
// tick, row ticks included, runs the channel effects (1000:03E6). A new
// pattern is taken from the order list when the row reaches 64; past the
// last order the song goes back to the restart order, or stops reading rows
// when there is none (the channels keep sounding).
//
// Pitch is an Amiga period times 4: note n plays period table[n + transpose]
// / C rate of the sample (table at DS:ED80, built by 1018:084B: the 12
// periods of octave 0 times 16 * 8363, halved per octave, 96 entries; C rate
// from the sample's finetune, table DS:00AB), and the channel's step is
// ((0xDA7A68 / period) << 16) / mix rate.
//
// Effects (command, parameter xy):
//   0 arpeggio (3 steps: the note, +x, +y semitones, one per tick);
//   1/2 period down/up by 4 * xy per tick (down to 0x71, no limit up);
//   3 tone portamento towards the note by 4 * xy (0: the last one);
//   4 vibrato, speed x, depth y (0: the last one), sine table DS:026E;
//   5/6 volume slide plus 3/4 with their last parameters;
//   9 sample offset xy * 256 (0: the last one);
//   A volume slide, +x then -y, every tick (the row tick included);
//   B order jump; C volume (at most 64); D pattern break to row 10x + y;
//   E9y retrigger every y ticks; ECy note cut after y ticks; EAy / EBy fine
//   volume up / down (once); F speed (below 0x20) or tempo;
//   1B retrigger (as E9).
// Not handled by the player (ignored): 7, 8 (panning, the output is mono),
// E1/E2, E5, ED and the others.

#include "core/amf_module.hpp"
#include "engine/audio/asylum_voice.hpp"

#include <array>
#include <cstdint>
#include <memory>

namespace crusader {

class AsylumMusic {
public:
    static constexpr int kChannels = AmfModule::kChannels;

    explicit AsylumMusic(int mix_rate);

    // 1008:004D (_ASS_PlayMusic): takes the module, not playing yet.
    void load(std::shared_ptr<const AmfModule> module);
    // _ASS_StartMusic / 1008:01AA: silences the channels and plays the
    // module from its first order.
    void start();
    // _ASS_StopMusic / 1008:01F4.
    void stop();
    bool playing() const { return playing_; }

    // One tick (1000:092A).
    void tick();
    // Samples per tick (DS:00A1).
    int tick_length() const { return tick_length_; }

    std::array<AsylumVoice, kChannels>& voices() { return voices_; }

    // Position, for tests: order index of the next pattern, row counter.
    int order_position() const { return order_pos_; }
    int row() const { return row_; }
    int period(int channel) const { return channels_[static_cast<std::size_t>(channel)].period; }

private:
    // The music fields of a mixer channel (after the voice part).
    struct Channel {
        int period = 0;                 // +17
        std::uint16_t effect = 0;       // +1D, command << 8 | parameter
        int target = 0;                 // +1F, tone portamento
        std::uint8_t porta_speed = 0;   // +21
        std::uint8_t vibrato_pos = 0;   // +22
        std::uint8_t vibrato = 0;       // +23
        std::uint8_t offset = 0;        // +24
        std::array<std::uint32_t, 3> arpeggio{};  // +25, steps
        int arpeggio_index = 0;         // +31
        int transpose = 0;              // +39
        std::uint8_t retrigger_count = 0;  // +3B, also the note cut counter
        std::uint8_t retrigger = 0;        // +3C
        std::uint32_t c_rate = 0;          // +3D
        const AmfSample* sample = nullptr;
    };

    std::uint32_t step_for(int period) const;
    void set_period(Channel& c, AsylumVoice& v, int period);
    void row_cell(Channel& c, AsylumVoice& v, const AmfCell& cell);  // 1000:062D
    void effect(Channel& c, AsylumVoice& v);  // 1000:03E6
    void tone_portamento(Channel& c, AsylumVoice& v, int speed);
    void vibrato(Channel& c, AsylumVoice& v, std::uint8_t param);
    static void volume_slide(AsylumVoice& v, std::uint8_t param);
    void retrigger(Channel& c, AsylumVoice& v);
    void set_tick_length_bpm(int tempo);

    int mix_rate_;
    std::shared_ptr<const AmfModule> module_;
    std::array<std::uint32_t, 96> periods_{};
    std::array<Channel, kChannels> channels_{};
    std::array<AsylumVoice, kChannels> voices_{};

    bool playing_ = false;     // DS:0097
    std::uint8_t order_pos_ = 0;   // DS:009B
    std::uint8_t speed_ = 6;       // DS:009C
    std::uint8_t counter_ = 6;     // DS:009D
    std::uint8_t row_ = 0x40;      // DS:009F
    std::uint8_t break_row_ = 0;   // DS:00A0
    int pattern_ = 0;              // DS:00A3, with
    int cell_row_ = 0;             // the row read next
    std::uint8_t restart_ = 0xFF;  // DS:028F
    int tick_length_;              // DS:00A1
};

}  // namespace crusader
