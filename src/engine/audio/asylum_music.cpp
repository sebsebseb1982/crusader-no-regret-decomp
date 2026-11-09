#include "engine/audio/asylum_music.hpp"

#include <algorithm>

namespace crusader {

namespace {

// Octave 0 periods (DS:0123).
constexpr std::array<std::uint32_t, 12> kOctave0 = {1712, 1616, 1524, 1440, 1356, 1280,
                                                    1208, 1140, 1076, 1016, 960,  907};
// C rate by finetune (DS:00AB): 0..7 up, 8..15 down.
constexpr std::array<std::uint32_t, 16> kCRates = {8363, 8413, 8463, 8529, 8581, 8651, 8723, 8757,
                                                   7895, 7941, 7985, 8046, 8107, 8169, 8232, 8280};
// Vibrato: half a sine (DS:026E).
constexpr std::array<std::uint8_t, 32> kSine = {0x00, 0x19, 0x32, 0x4A, 0x62, 0x78, 0x8E, 0xA2, 0xB4, 0xC5, 0xD4,
                                                0xE1, 0xEC, 0xF4, 0xFA, 0xFE, 0xFF, 0xFE, 0xFA, 0xF4, 0xEC, 0xE1,
                                                0xD4, 0xC5, 0xB4, 0xA2, 0x8E, 0x78, 0x62, 0x4A, 0x32, 0x19};
constexpr std::uint32_t kClock = 0xDA7A68;  // 4 * the Amiga clock
constexpr int kMinPeriod = 0x71;
constexpr int kMaxPeriod = 0x6B00;
constexpr int kMaxVolume = 0x40;
constexpr int kPlayerSamples = 31;  // 1008:004D copies 31 of the 64 samples
constexpr std::uint8_t kNoOrder = 0xFF;
constexpr std::uint16_t kSpentEffect = 0x0EFF;  // after EAy / EBy: nothing

// The ±values of the player are 16-bit registers.
int as_i16(int v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v)); }

}  // namespace

AsylumMusic::AsylumMusic(int mix_rate) : mix_rate_(mix_rate), tick_length_(mix_rate / 50) {
    for (std::size_t i = 0; i < periods_.size(); ++i) periods_[i] = (kOctave0[i % 12] * 16 * 8363) >> (i / 12);
}

// @orig ?: step computation of the Asylum player, not located
std::uint32_t AsylumMusic::step_for(int period) const {
    const auto p = static_cast<std::uint16_t>(period);
    if (p == 0) return 0;
    // The shift drops the high bits, as the 32-bit register does.
    return ((kClock / p) << 16) / static_cast<std::uint32_t>(mix_rate_);
}

void AsylumMusic::set_period(Channel& c, AsylumVoice& v, int period) {
    c.period = static_cast<std::uint16_t>(period);
    v.step = step_for(c.period);
}

void AsylumMusic::set_tick_length_bpm(int tempo) {
    // 1008:004D and 1008:01AA: BPM * 24 / 60 ticks per second.
    const int per_second = static_cast<std::int8_t>(tempo) * 24 / 60;
    if (per_second != 0) tick_length_ = mix_rate_ / static_cast<std::uint16_t>(per_second);
}

// @orig ASYLUM.DLL:1018:0772 ASYLUM.DLL:1008:004D
void AsylumMusic::load(std::shared_ptr<const AmfModule> module) {
    playing_ = false;
    module_ = std::move(module);
    if (!module_) return;
    speed_ = module_->speed;
    set_tick_length_bpm(module_->tempo);
    restart_ = module_->restart < module_->order_count ? module_->restart : kNoOrder;
}

// @orig ASYLUM.DLL:1018:024A ASYLUM.DLL:1008:01AA
void AsylumMusic::start() {
    for (AsylumVoice& v : voices_) v.active = false;
    if (!module_) return;
    order_pos_ = 0;
    speed_ = module_->speed;
    counter_ = speed_;
    row_ = AmfModule::kRows;
    break_row_ = 0;
    set_tick_length_bpm(module_->tempo);
    playing_ = true;
}

// @orig ASYLUM.DLL:1018:0286 ASYLUM.DLL:1008:01F4
void AsylumMusic::stop() {
    playing_ = false;
    module_.reset();
    order_pos_ = 0;
    pattern_ = 0;
    cell_row_ = 0;
    speed_ = 6;
    counter_ = 6;
    row_ = AmfModule::kRows;
    break_row_ = 0;
    tick_length_ = mix_rate_ / 50;
    for (AsylumVoice& v : voices_) v.active = false;
}

// @orig ASYLUM.DLL:1000:092A
void AsylumMusic::tick() {
    if (--counter_ != 0) {
        for (int i = 0; i < kChannels; ++i) effect(channels_[i], voices_[i]);
        return;
    }
    counter_ = speed_;
    if (!module_) return;
    if (row_ >= AmfModule::kRows) {
        std::uint8_t order = order_pos_;
        if (order >= module_->order_count) {
            order = restart_;
            order_pos_ = order;
            if (order >= module_->order_count) return;  // song over
        }
        pattern_ = module_->orders[order];
        // The original sets the row counter to the break row but reads the
        // new pattern from its first row: a break shortens the pattern.
        cell_row_ = 0;
        row_ = break_row_;
        break_row_ = 0;
        ++order_pos_;
    }
    ++row_;
    for (int i = 0; i < kChannels; ++i) row_cell(channels_[i], voices_[i], module_->cell(pattern_, cell_row_, i));
    ++cell_row_;
    for (int i = 0; i < kChannels; ++i) effect(channels_[i], voices_[i]);
}

// @orig ASYLUM.DLL:1000:062D
void AsylumMusic::row_cell(Channel& c, AsylumVoice& v, const AmfCell& cell) {
    c.effect = static_cast<std::uint16_t>(cell.command << 8 | cell.param);
    if (cell.sample != 0) {
        // Samples past the 31 the player copies are empty.
        static const AmfSample kEmpty;
        const AmfSample& s = cell.sample <= kPlayerSamples ? module_->samples[cell.sample - 1u] : kEmpty;
        v.volume = s.volume;
        v.pcm = s.pcm;
        v.length = s.length & 0xFFFF;
        v.loop_start = s.loop_start & 0xFFFF;
        v.loop_length = s.loop_length & 0xFFFF;
        c.transpose = s.transpose;
        c.c_rate = &s == &kEmpty ? 0 : kCRates[s.finetune & 0xF];
        v.active = true;
    }
    const int index = cell.note + c.transpose;
    const bool note = cell.note != 0 && c.c_rate != 0 && index >= 0 && index < static_cast<int>(periods_.size());
    if (note) {
        c.target = static_cast<std::uint16_t>(periods_[static_cast<std::size_t>(index)] / c.c_rate);
        if (cell.command != 3) {
            set_period(c, v, c.target);
            v.pos = 0;
        }
    }
    if (c.effect == 0) return;
    const std::uint8_t p = cell.param;
    const int x = p >> 4;
    const int y = p & 0xF;
    switch (cell.command) {
    case 0x0: {  // arpeggio: the steps of the nearest note, +x and +y
        const std::uint32_t at = static_cast<std::uint32_t>(c.period) * c.c_rate;
        std::size_t base = 0;
        while (base < periods_.size() && at < periods_[base]) ++base;
        const auto step_of = [&](std::size_t i) {
            i = std::min(i, periods_.size() - 1);  // the original reads past the table
            return c.c_rate ? step_for(static_cast<int>(periods_[i] / c.c_rate)) : 0;
        };
        c.arpeggio = {step_of(base), step_of(base + x), step_of(base + y)};
        c.arpeggio_index = 0;
        break;
    }
    case 0x3:
        c.porta_speed = p ? p : c.porta_speed;
        c.effect = static_cast<std::uint16_t>(0x300 | c.porta_speed);
        break;
    case 0x4: {
        std::uint8_t v4 = p;
        if ((v4 & 0x0F) == 0) v4 |= c.vibrato & 0x0F;
        if ((v4 & 0xF0) == 0) v4 |= c.vibrato & 0xF0;
        c.vibrato = v4;
        c.effect = static_cast<std::uint16_t>(0x400 | v4);
        if (note) c.vibrato_pos = 0;
        break;
    }
    case 0x9: {
        c.offset = p ? p : c.offset;
        const std::int32_t pos = c.offset << 8;
        v.pos = static_cast<std::uint32_t>(pos < static_cast<std::int32_t>(v.length) ? pos : static_cast<std::int32_t>(v.length) - 1);
        break;
    }
    case 0xB:
        order_pos_ = p;
        row_ = AmfModule::kRows;
        break;
    case 0xC:
        v.volume = std::min<int>(p, kMaxVolume);
        break;
    case 0xD:
        break_row_ = static_cast<std::uint8_t>(x * 10 + y);
        row_ = AmfModule::kRows;
        break;
    case 0xE:
        if (x == 0xC) {
            c.retrigger_count = static_cast<std::uint8_t>(y);
        } else if (x == 0x9) {
            c.retrigger = static_cast<std::uint8_t>(y);
            c.retrigger_count = static_cast<std::uint8_t>(y);
        }
        break;
    case 0xF:
        if (p == 0) break;
        if (p < 0x20) {
            speed_ = p;
            counter_ = p;
        } else {
            const int per_second = p * 0x66 >> 8;
            tick_length_ = mix_rate_ / per_second;
        }
        break;
    case 0x1B:
        if (p == 0) {
            c.retrigger_count = c.retrigger;
        } else {
            c.retrigger = static_cast<std::uint8_t>(y);
            c.retrigger_count = static_cast<std::uint8_t>(y);
        }
        break;
    default:
        break;
    }
}

void AsylumMusic::volume_slide(AsylumVoice& v, std::uint8_t param) {
    const int down = std::max(v.volume - (param & 0xF), 0);
    v.volume = std::min(down + (param >> 4), kMaxVolume);
}

// @orig ASYLUM.DLL:1000:03E6
void AsylumMusic::tone_portamento(Channel& c, AsylumVoice& v, int speed) {
    const int target = as_i16(c.target);
    int period = as_i16(c.period);
    if (period == target) return;
    if (period > target) {
        period = std::max(as_i16(period - speed * 4), target);
    } else {
        period = std::min(as_i16(period + speed * 4), target);
    }
    set_period(c, v, period);
}

// @orig ASYLUM.DLL:1000:03E6
void AsylumMusic::vibrato(Channel& c, AsylumVoice& v, std::uint8_t param) {
    c.vibrato_pos = static_cast<std::uint8_t>(c.vibrato_pos + (param >> 4) * 4);
    int delta = kSine[(c.vibrato_pos >> 2) & 0x1F] * (param & 0xF) >> 7;
    if (c.vibrato_pos & 0x80) delta = -delta;
    const int period = std::clamp(as_i16(c.period + delta), kMinPeriod, kMaxPeriod);
    v.step = step_for(period);  // the channel's period stays
}

void AsylumMusic::retrigger(Channel& c, AsylumVoice& v) {
    if (c.retrigger_count != 0 && --c.retrigger_count != 0) return;
    v.pos = 0;
    c.retrigger_count = c.retrigger;
}

// @orig ASYLUM.DLL:1000:03E6
void AsylumMusic::effect(Channel& c, AsylumVoice& v) {
    if (c.effect == 0) return;
    const std::uint8_t p = c.effect & 0xFF;
    switch (c.effect >> 8) {
    case 0x0:
        v.step = c.arpeggio[static_cast<std::size_t>(c.arpeggio_index)];
        c.arpeggio_index = (c.arpeggio_index + 1) % 3;
        break;
    case 0x1:
        set_period(c, v, std::max(as_i16(c.period - p * 4), kMinPeriod));
        break;
    case 0x2:
        set_period(c, v, c.period + p * 4);
        break;
    case 0x3:
        tone_portamento(c, v, p);
        break;
    case 0x4:
        vibrato(c, v, p);
        break;
    case 0x5:
        volume_slide(v, p);
        tone_portamento(c, v, c.porta_speed);
        break;
    case 0x6:
        volume_slide(v, p);
        vibrato(c, v, c.vibrato);
        break;
    case 0xA:
        volume_slide(v, p);
        break;
    case 0xE:
        switch (p >> 4) {
        case 0xC:  // note cut
            if (c.retrigger_count != 0 && --c.retrigger_count != 0) break;
            v.volume = 0;
            break;
        case 0x9:
            retrigger(c, v);
            break;
        case 0xA:
            v.volume = std::min(v.volume + (p & 0xF), kMaxVolume);
            c.effect = kSpentEffect;
            break;
        case 0xB:
            v.volume = std::max(v.volume - (p & 0xF), 0);
            c.effect = kSpentEffect;
            break;
        default:
            break;
        }
        break;
    case 0x1B:
        retrigger(c, v);
        break;
    default:
        break;
    }
}

}  // namespace crusader
