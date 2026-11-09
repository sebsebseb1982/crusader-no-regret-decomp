// Renders every frame of one shape into a PNG contact sheet, to check the
// shape decoder against the original game's rendering.
//
// Usage: shapedump <archive.FLX | file.SHP> <shape#> <palette.PAL> <out.png>

#include "core/flex.hpp"
#include "core/palette.hpp"
#include "core/png_writer.hpp"
#include "core/shape.hpp"

#include <algorithm>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <string>

using namespace crusader;

int main(int argc, char** argv) {
    if (argc != 5) {
        std::fprintf(stderr, "usage: shapedump <archive.FLX> <shape#> <palette.PAL> <out.png>\n");
        return 2;
    }
    try {
        // A lone .SHP file (MOUSE.SHP) is a shape by itself; <shape#> is ignored.
        const std::filesystem::path path = argv[1];
        const bool lone = path.extension() == ".SHP" || path.extension() == ".shp";
        const auto bytes = lone ? read_file(path) : std::vector<std::uint8_t>{};
        const auto flex = lone ? FlexArchive{} : FlexArchive::open(path);
        const auto shape = Shape::parse(lone ? std::span<const std::uint8_t>(bytes) : flex.entry(std::stoul(argv[2])),
                                        layout_for_archive(path.filename().string()));
        const auto palette = Palette::load(argv[3]);
        const auto& frames = shape.frames();
        if (frames.empty()) {
            std::fprintf(stderr, "shape has no frames\n");
            return 1;
        }

        // Grid of cells large enough for any frame, each drawn at its anchor.
        int max_left = 0, max_right = 0, max_up = 0, max_down = 0;
        for (const auto& f : frames) {
            max_left = std::max(max_left, f.xoff);
            max_right = std::max(max_right, f.width - f.xoff);
            max_up = std::max(max_up, f.yoff);
            max_down = std::max(max_down, f.height - f.yoff);
        }
        const int cell_w = max_left + max_right + 2;
        const int cell_h = max_up + max_down + 2;
        const int cols = std::min<int>(16, static_cast<int>(frames.size()));
        const int rows = (static_cast<int>(frames.size()) + cols - 1) / cols;

        Surface8 sheet(cell_w * cols, cell_h * rows, kTransparentIndex);
        for (std::size_t i = 0; i < frames.size(); ++i) {
            const int cx = static_cast<int>(i % cols) * cell_w + 1 + max_left;
            const int cy = static_cast<int>(i / cols) * cell_h + 1 + max_up;
            frames[i].draw(sheet, cx, cy);
        }
        write_png(argv[4], sheet, palette, kTransparentIndex);
        std::printf("%zu frames -> %s (%dx%d)\n", frames.size(), argv[4], sheet.width, sheet.height);
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
}
