// Interactive shape browser: first visible result of the SDL port.
//
// Usage: shapeview [data_dir] [archive relative to data_dir]
//   Left/Right   previous/next shape      PageUp/PageDown  -/+ 50 shapes
//   Up/Down      previous/next frame      M  mirror         Esc  quit

#define SDL_MAIN_HANDLED
#include "core/flex.hpp"
#include "core/palette.hpp"
#include "core/shape.hpp"
#include "platform/sdl_presenter.hpp"

#include <SDL.h>

#include <cstdio>
#include <exception>
#include <filesystem>
#include <optional>
#include <string>

using namespace crusader;

namespace {

constexpr int kScreenW = 640;  // original VESA mode
constexpr int kScreenH = 480;
constexpr std::uint8_t kBackground = 0;

struct Browser {
    const FlexArchive& flex;
    ShapeLayout layout;
    int shape_no = 1;
    int frame_no = 0;
    bool mirrored = false;
    std::optional<Shape> shape;
    std::string error;

    void load(int delta) {
        const int n = static_cast<int>(flex.count());
        for (int tries = 0; tries < n; ++tries) {
            shape_no = ((shape_no + delta) % n + n) % n;
            if (!flex.entry(shape_no).empty()) {
                break;
            }
            if (delta == 0) delta = 1;
        }
        frame_no = 0;
        try {
            shape = Shape::parse(flex.entry(shape_no), layout);
            error.clear();
        } catch (const std::exception& e) {
            shape.reset();
            error = e.what();
        }
    }

    std::string title() const {
        std::string t = "shape " + std::to_string(shape_no);
        if (shape) {
            t += "  frame " + std::to_string(frame_no) + "/" + std::to_string(shape->frames().size());
            if (!shape->frames().empty()) {
                const auto& f = shape->frames()[frame_no];
                t += "  " + std::to_string(f.width) + "x" + std::to_string(f.height);
            }
        } else {
            t += "  error: " + error;
        }
        return t;
    }
};

}  // namespace

int main(int argc, char** argv) {
    SDL_SetMainReady();
    const std::filesystem::path data = argc > 1 ? argv[1] : "data";
    const std::filesystem::path archive = argc > 2 ? argv[2] : "STATIC/SHAPES.FLX";
    try {
        const auto flex = FlexArchive::open(data / archive);
        const auto palette = Palette::load(data / "STATIC/GAMEPAL.PAL");
        SdlPresenter presenter("shapeview", kScreenW, kScreenH, 2);

        Browser b{flex, layout_for_archive(archive.filename().string())};
        b.load(0);
        Surface8 screen(kScreenW, kScreenH);
        bool running = true;
        while (running) {
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_QUIT) running = false;
                if (ev.type != SDL_KEYDOWN) continue;
                const int frames = b.shape ? static_cast<int>(b.shape->frames().size()) : 0;
                switch (ev.key.keysym.sym) {
                    case SDLK_ESCAPE: running = false; break;
                    case SDLK_RIGHT: b.load(1); break;
                    case SDLK_LEFT: b.load(-1); break;
                    case SDLK_PAGEDOWN: b.load(50); break;
                    case SDLK_PAGEUP: b.load(-50); break;
                    case SDLK_DOWN: if (frames) b.frame_no = (b.frame_no + 1) % frames; break;
                    case SDLK_UP: if (frames) b.frame_no = (b.frame_no + frames - 1) % frames; break;
                    case SDLK_m: b.mirrored = !b.mirrored; break;
                    default: break;
                }
            }
            screen.clear(kBackground);
            if (b.shape && !b.shape->frames().empty()) {
                b.shape->frames()[b.frame_no].draw(screen, kScreenW / 2, kScreenH * 2 / 3, b.mirrored);
            }
            presenter.set_title(b.title());
            presenter.present(screen, palette);
        }
        SDL_Quit();
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "shapeview", e.what(), nullptr);
        return 1;
    }
}
