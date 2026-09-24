#include "ui/wheel_geometry.h"

#include <algorithm>
#include <cmath>

namespace ui::geometry {

float NormalizeAngle(float angle) {
    angle = std::fmod(angle + kPi, kTwoPi);
    if (angle < 0.0f) {
        angle += kTwoPi;
    }
    return angle - kPi;
}

bool AngleInSector(float angle, const Sector &sector) {
    const float width = sector.end - sector.start;
    const float offset = NormalizeAngle(angle - sector.start);
    const float wrapped = offset < 0.0f ? offset + kTwoPi : offset;
    return wrapped <= width;
}

Sector CategorySector(int index, int count, float gap) {
    const float share = kTwoPi / static_cast<float>(count);
    const float middle = kTop + share * static_cast<float>(index);
    return Sector{middle - 0.5f * share + 0.5f * gap,
                  middle + 0.5f * share - 0.5f * gap};
}

Sector ItemSector(int index, int count, float centerAngle, float itemSpan,
                  float maxSpan, float gap) {
    const float span =
        std::min(itemSpan * static_cast<float>(count), maxSpan);
    const float share = span / static_cast<float>(count);
    const float start = centerAngle - 0.5f * span +
                        share * static_cast<float>(index);
    return Sector{start + 0.5f * gap, start + share - 0.5f * gap};
}

Hit HitTest(const Layout &layout, float dx, float dy) {
    const float radius = std::sqrt(dx * dx + dy * dy);
    if (radius <= layout.centerRadius) {
        return Hit{Band::Center, -1};
    }

    const float angle = std::atan2(dy, dx);
    if (radius >= layout.items.inner && radius <= layout.items.outer) {
        for (int item = 0; item < layout.itemCount; ++item) {
            if (AngleInSector(angle, ItemSector(item, layout.itemCount,
                                                layout.itemCenter,
                                                layout.itemSpan,
                                                layout.itemMaxSpan,
                                                layout.itemGap))) {
                return Hit{Band::Items, item};
            }
        }
        return Hit{};
    }

    if (radius >= layout.categories.inner &&
        radius <= layout.categories.outer) {
        for (int category = 0; category < layout.categoryCount; ++category) {
            if (AngleInSector(angle, CategorySector(category,
                                                    layout.categoryCount,
                                                    layout.categoryGap))) {
                return Hit{Band::Categories, category};
            }
        }
    }
    return Hit{};
}

float EaseOutCubic(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    const float inverse = 1.0f - t;
    return 1.0f - inverse * inverse * inverse;
}

} // namespace ui::geometry
