#pragma once

namespace ui::geometry {

inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr float kTwoPi = 2.0f * kPi;
// Screen space has y growing downward, so angles grow clockwise and the top
// of the wheel is -pi/2.
inline constexpr float kTop = -0.5f * kPi;

struct Sector {
    float start = 0.0f;
    float end = 0.0f;

    float Middle() const { return 0.5f * (start + end); }
};

struct Ring {
    float inner = 0.0f;
    float outer = 0.0f;
};

enum class Band { None, Center, Items, Categories };

struct Hit {
    Band band = Band::None;
    int index = -1;
};

// Wraps an angle into [-pi, pi).
float NormalizeAngle(float angle);

bool AngleInSector(float angle, const Sector &sector);

// Category `index` of `count` around the full circle, the first centered at
// the top, with `gap` radians left empty between neighbors.
Sector CategorySector(int index, int count, float gap);

// Item `index` of `count` in a fan centered on `centerAngle`. Each item gets
// `itemSpan` radians, the fan never exceeds `maxSpan`, and `gap` separates
// neighbors.
Sector ItemSector(int index, int count, float centerAngle, float itemSpan,
                  float maxSpan, float gap);

struct Layout {
    float centerRadius = 0.0f;
    Ring items;
    Ring categories;
    int categoryCount = 0;
    float categoryGap = 0.0f;
    int itemCount = 0;
    float itemCenter = 0.0f;
    float itemSpan = 0.0f;
    float itemMaxSpan = 0.0f;
    float itemGap = 0.0f;
};

// Which band and segment a point offset (dx, dy) from the wheel center falls
// in. Points in a gap or outside every ring return Band::None.
Hit HitTest(const Layout &layout, float dx, float dy);

float EaseOutCubic(float t);

} // namespace ui::geometry
