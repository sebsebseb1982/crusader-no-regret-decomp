#include "engine/ui/difficulty_screen.hpp"

#include "core/language.hpp"
#include "engine/ui/gump_keys.hpp"

namespace crusader {

namespace {

constexpr std::array<int, DifficultyScreen::kLevels> kButtonTops = {120, 179, 238, 297};
constexpr int kButtonLeft = 159, kButtonWidth = 321;  // x1 forced to 320, then centred
constexpr int kSecondHalf = 170;                       // frame 1 to the right of frame 0

}  // namespace

// @orig 13B8:27F9 13B8:2FAC
DifficultyScreen::DifficultyScreen(ShapeCache& gumps, int& selected, Audio* audio)
    : gumps_(&gumps), selected_(&selected), audio_(audio) {
    for (int level = 1; level <= kLevels; ++level) {
        const Shape* s = gumps_->get(static_cast<std::uint16_t>(kBackground + level));
        const int h = s && !s->frames().empty() ? s->frames()[0].height : 1;
        const int top = kButtonTops[level - 1];
        buttons_[level - 1] = {kButtonLeft, top, kButtonLeft + kButtonWidth - 1, top + h - 1};
    }
}

void DifficultyScreen::draw_shape(Surface8& screen, std::uint16_t shape, int frame, int x, int y) const {
    const Shape* s = gumps_->get(shape);
    if (s && static_cast<std::size_t>(frame) < s->frames().size()) s->frames()[frame].draw(screen, x, y);
}

// @orig 13B8:2D79 13B8:3080
void DifficultyScreen::draw(Surface8& screen) const {
    screen.clear(0);
    const bool regret = regret_data();
    if (us_version() || regret) {
        // The US shape has one frame, centred (13D0:2B16 of the US executable).
        const Shape* s = gumps_->get(kBackground);
        const int w = s && !s->frames().empty() ? s->frames()[0].width : 0;
        draw_shape(screen, kBackground, 0, (640 - w) >> 1, 76);
    } else {
        draw_shape(screen, kBackground, 0, 139, 76);
        draw_shape(screen, kBackground, 1, 331, 76);
    }
    for (int level = 1; level <= kLevels; ++level) {
        const Rect& r = buttons_[level - 1];
        const auto shape = static_cast<std::uint16_t>(kBackground + level);
        draw_shape(screen, shape, 0, r.x0, r.y0);
        draw_shape(screen, shape, 1, r.x0 + kSecondHalf, r.y0);
        if (regret) outline(screen, r.x0, r.y0, r.x1, r.y1, level == *selected_ ? kRegretSelectedColour : kRegretColour);
        else if (level == *selected_) outline(screen, r.x0, r.y0, r.x1, r.y1, kOutlineColour);
    }
}

void DifficultyScreen::select(int level) {
    *selected_ = level;
    if (!regret_data() || !audio_) return;
    audio_->stop_all();
    audio_->play_sfx(kRegretFirstSound + level - 1);
}

// @orig 13B8:2C16
void DifficultyScreen::key(std::uint16_t code) {
    if (result_) return;
    int& sel = *selected_;
    switch (code) {
    case gump_key::kUp:
    case '8':
        if (sel > 1) select(sel - 1);
        break;
    case gump_key::kDown:
    case '2':
        if (sel < kLevels) select(sel + 1);
        break;
    case gump_key::kEnter:
        result_ = sel;
        break;
    case gump_key::kEscape:
        result_ = 0;
        break;
    default:
        break;
    }
}

// @orig 1308:001D
void DifficultyScreen::mouse_down(int x, int y) {
    if (result_ || captured_) return;
    for (int level = 1; level <= kLevels; ++level) {
        if (buttons_[level - 1].contains(x, y)) {
            captured_ = level;
            select(level);
            return;
        }
    }
}

void DifficultyScreen::mouse_move(int x, int y) {
    if (captured_ && buttons_[captured_ - 1].contains(x, y)) *selected_ = captured_;
}

// @orig 1308:001D 13B8:2BD8
void DifficultyScreen::mouse_up(int x, int y) {
    if (result_ || !captured_) return;
    const int level = captured_;
    captured_ = 0;
    if (buttons_[level - 1].contains(x, y)) result_ = level;
}

}  // namespace crusader
