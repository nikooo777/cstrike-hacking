#pragma once

#include <vector>

#include "imgui.h"

namespace preview {

struct Canvas {
    int width = 0;
    int height = 0;
    std::vector<float> rgb;

    Canvas(int canvasWidth, int canvasHeight);
    void Set(int x, int y, float r, float g, float b);
};

struct Texture {
    int width = 0;
    int height = 0;
    const unsigned char *rgba = nullptr;
};

// Software-renders ImGui draw data the way the DX9 backend does: textured,
// vertex-colored triangles, alpha-blended, clipped to each command's rect.
void Rasterize(const ImDrawData &drawData, const Texture &texture,
               Canvas &canvas);

std::vector<unsigned char> Crop(const Canvas &canvas, int left, int top,
                                int width, int height);

} // namespace preview
