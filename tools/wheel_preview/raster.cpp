#include "raster.h"

#include <algorithm>
#include <cmath>

namespace preview {

namespace {

struct Color {
    float r, g, b, a;
};

Color Unpack(ImU32 color) {
    return Color{static_cast<float>((color >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f,
                 static_cast<float>((color >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f,
                 static_cast<float>((color >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f,
                 static_cast<float>((color >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f};
}

float Edge(const ImVec2 &a, const ImVec2 &b, float x, float y) {
    return (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
}

// Exactly one of two triangles sharing an edge owns the pixels on it, because
// they traverse the edge in opposite directions.
bool OwnsEdge(const ImVec2 &a, const ImVec2 &b) {
    const float dy = b.y - a.y;
    return dy < 0.0f || (dy == 0.0f && b.x - a.x > 0.0f);
}

bool Covered(float weight, bool owned) {
    return weight > 0.0f || (weight == 0.0f && owned);
}

Color Sample(const Texture &texture, float u, float v) {
    const int x = std::clamp(static_cast<int>(u * static_cast<float>(texture.width)), 0,
                             texture.width - 1);
    const int y = std::clamp(static_cast<int>(v * static_cast<float>(texture.height)), 0,
                             texture.height - 1);
    const unsigned char *texel = texture.rgba + (y * texture.width + x) * 4;
    return Color{texel[0] / 255.0f, texel[1] / 255.0f, texel[2] / 255.0f,
                 texel[3] / 255.0f};
}

void Triangle(const ImDrawVert &first, const ImDrawVert &second,
              const ImDrawVert &third, const ImVec4 &clip,
              const Texture &texture, Canvas &canvas) {
    const ImDrawVert *vertices[3] = {&first, &second, &third};
    float area = Edge(first.pos, second.pos, third.pos.x, third.pos.y);
    if (std::fabs(area) < 1e-8f) {
        return;
    }
    if (area < 0.0f) {
        std::swap(vertices[1], vertices[2]);
        area = -area;
    }
    const ImVec2 &p0 = vertices[0]->pos;
    const ImVec2 &p1 = vertices[1]->pos;
    const ImVec2 &p2 = vertices[2]->pos;
    const bool own0 = OwnsEdge(p1, p2);
    const bool own1 = OwnsEdge(p2, p0);
    const bool own2 = OwnsEdge(p0, p1);
    const Color c0 = Unpack(vertices[0]->col);
    const Color c1 = Unpack(vertices[1]->col);
    const Color c2 = Unpack(vertices[2]->col);

    const int minX = std::max(static_cast<int>(std::floor(std::min({p0.x, p1.x, p2.x}))),
                              std::max(0, static_cast<int>(clip.x)));
    const int maxX = std::min(static_cast<int>(std::ceil(std::max({p0.x, p1.x, p2.x}))),
                              std::min(canvas.width, static_cast<int>(clip.z)));
    const int minY = std::max(static_cast<int>(std::floor(std::min({p0.y, p1.y, p2.y}))),
                              std::max(0, static_cast<int>(clip.y)));
    const int maxY = std::min(static_cast<int>(std::ceil(std::max({p0.y, p1.y, p2.y}))),
                              std::min(canvas.height, static_cast<int>(clip.w)));

    for (int y = minY; y < maxY; ++y) {
        for (int x = minX; x < maxX; ++x) {
            const float px = static_cast<float>(x) + 0.5f;
            const float py = static_cast<float>(y) + 0.5f;
            const float w0 = Edge(p1, p2, px, py);
            const float w1 = Edge(p2, p0, px, py);
            const float w2 = Edge(p0, p1, px, py);
            if (!Covered(w0, own0) || !Covered(w1, own1) || !Covered(w2, own2)) {
                continue;
            }
            const float b0 = w0 / area;
            const float b1 = w1 / area;
            const float b2 = w2 / area;
            const float u = b0 * vertices[0]->uv.x + b1 * vertices[1]->uv.x +
                            b2 * vertices[2]->uv.x;
            const float v = b0 * vertices[0]->uv.y + b1 * vertices[1]->uv.y +
                            b2 * vertices[2]->uv.y;
            const Color texel = Sample(texture, u, v);
            const float r = (b0 * c0.r + b1 * c1.r + b2 * c2.r) * texel.r;
            const float g = (b0 * c0.g + b1 * c1.g + b2 * c2.g) * texel.g;
            const float b = (b0 * c0.b + b1 * c1.b + b2 * c2.b) * texel.b;
            const float a = (b0 * c0.a + b1 * c1.a + b2 * c2.a) * texel.a;
            float *pixel = &canvas.rgb[(y * canvas.width + x) * 3];
            pixel[0] = r * a + pixel[0] * (1.0f - a);
            pixel[1] = g * a + pixel[1] * (1.0f - a);
            pixel[2] = b * a + pixel[2] * (1.0f - a);
        }
    }
}

} // namespace

Canvas::Canvas(int canvasWidth, int canvasHeight)
    : width(canvasWidth), height(canvasHeight),
      rgb(static_cast<std::size_t>(canvasWidth * canvasHeight * 3), 0.0f) {}

void Canvas::Set(int x, int y, float r, float g, float b) {
    float *pixel = &rgb[(y * width + x) * 3];
    pixel[0] = r;
    pixel[1] = g;
    pixel[2] = b;
}

void Rasterize(const ImDrawData &drawData, const Texture &texture,
               Canvas &canvas) {
    for (int list = 0; list < drawData.CmdListsCount; ++list) {
        const ImDrawList *commands = drawData.CmdLists[list];
        for (const ImDrawCmd &command : commands->CmdBuffer) {
            if (command.UserCallback != nullptr) {
                continue;
            }
            const ImVec4 clip(command.ClipRect.x - drawData.DisplayPos.x,
                              command.ClipRect.y - drawData.DisplayPos.y,
                              command.ClipRect.z - drawData.DisplayPos.x,
                              command.ClipRect.w - drawData.DisplayPos.y);
            for (unsigned int element = 0; element < command.ElemCount;
                 element += 3) {
                const ImDrawIdx *indices =
                    commands->IdxBuffer.Data + command.IdxOffset + element;
                const ImDrawVert *vertices =
                    commands->VtxBuffer.Data + command.VtxOffset;
                Triangle(vertices[indices[0]], vertices[indices[1]],
                         vertices[indices[2]], clip, texture, canvas);
            }
        }
    }
}

std::vector<unsigned char> Crop(const Canvas &canvas, int left, int top,
                                int width, int height) {
    std::vector<unsigned char> bytes(static_cast<std::size_t>(width * height * 3));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const float *pixel =
                &canvas.rgb[((top + y) * canvas.width + (left + x)) * 3];
            for (int channel = 0; channel < 3; ++channel) {
                bytes[(y * width + x) * 3 + channel] = static_cast<unsigned char>(
                    std::clamp(pixel[channel], 0.0f, 1.0f) * 255.0f + 0.5f);
            }
        }
    }
    return bytes;
}

} // namespace preview
