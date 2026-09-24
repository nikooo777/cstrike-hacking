#include <cmath>
#include <iostream>

#include "ui/wheel_geometry.h"

namespace {

namespace geometry = ui::geometry;

int g_failures = 0;

void Check(bool condition, const char *expression, int line) {
    if (!condition) {
        std::cerr << "FAIL line " << line << ": " << expression << '\n';
        ++g_failures;
    }
}

#define CHECK(expression) Check((expression), #expression, __LINE__)

bool Near(float lhs, float rhs, float epsilon = 1e-5f) {
    return std::fabs(lhs - rhs) <= epsilon;
}

constexpr float kDegree = geometry::kPi / 180.0f;

geometry::Layout SampleLayout(int selectedCategory) {
    geometry::Layout layout{};
    layout.centerRadius = 100.0f;
    layout.items = {110.0f, 160.0f};
    layout.categories = {170.0f, 200.0f};
    layout.categoryCount = 5;
    layout.categoryGap = 2.0f * kDegree;
    layout.itemCount = 3;
    layout.itemCenter =
        geometry::CategorySector(selectedCategory, 5, layout.categoryGap).Middle();
    layout.itemSpan = 36.0f * kDegree;
    layout.itemMaxSpan = 150.0f * kDegree;
    layout.itemGap = 1.0f * kDegree;
    return layout;
}

void Point(float radius, float angle, float &dx, float &dy) {
    dx = radius * std::cos(angle);
    dy = radius * std::sin(angle);
}

void TestAngles() {
    CHECK(Near(geometry::NormalizeAngle(0.0f), 0.0f));
    CHECK(Near(geometry::NormalizeAngle(geometry::kTwoPi + 0.5f), 0.5f));
    CHECK(Near(geometry::NormalizeAngle(-geometry::kTwoPi - 0.5f), -0.5f));
    CHECK(Near(geometry::NormalizeAngle(geometry::kPi), -geometry::kPi));

    const geometry::Sector wrapping{170.0f * kDegree, 190.0f * kDegree};
    CHECK(geometry::AngleInSector(175.0f * kDegree, wrapping));
    CHECK(geometry::AngleInSector(-175.0f * kDegree, wrapping));
    CHECK(!geometry::AngleInSector(160.0f * kDegree, wrapping));
    CHECK(!geometry::AngleInSector(0.0f, wrapping));

    CHECK(geometry::EaseOutCubic(-1.0f) == 0.0f);
    CHECK(geometry::EaseOutCubic(0.0f) == 0.0f);
    CHECK(geometry::EaseOutCubic(1.0f) == 1.0f);
    CHECK(geometry::EaseOutCubic(2.0f) == 1.0f);
    CHECK(geometry::EaseOutCubic(0.5f) > 0.5f);
}

void TestSectors() {
    const float gap = 2.0f * kDegree;
    const auto first = geometry::CategorySector(0, 5, gap);
    CHECK(Near(first.Middle(), geometry::kTop));
    CHECK(Near(first.end - first.start, 72.0f * kDegree - gap));

    const auto second = geometry::CategorySector(1, 5, gap);
    CHECK(Near(second.Middle() - first.Middle(), 72.0f * kDegree));
    CHECK(Near(second.start - first.end, gap));

    const float span = 36.0f * kDegree;
    const float cap = 150.0f * kDegree;
    const auto lone = geometry::ItemSector(0, 1, 0.0f, span, cap, 0.0f);
    CHECK(Near(lone.Middle(), 0.0f));
    CHECK(Near(lone.end - lone.start, span));

    const auto capped = geometry::ItemSector(0, 5, 0.0f, span, cap, 0.0f);
    const auto lastCapped = geometry::ItemSector(4, 5, 0.0f, span, cap, 0.0f);
    CHECK(Near(lastCapped.end - capped.start, cap));
    CHECK(Near(0.5f * (capped.start + lastCapped.end), 0.0f));
}

void TestHitTest() {
    const auto layout = SampleLayout(1);
    float dx = 0.0f;
    float dy = 0.0f;

    auto hit = geometry::HitTest(layout, 0.0f, 0.0f);
    CHECK(hit.band == geometry::Band::Center);

    Point(185.0f, geometry::kTop, dx, dy);
    hit = geometry::HitTest(layout, dx, dy);
    CHECK(hit.band == geometry::Band::Categories && hit.index == 0);

    const float boundary = geometry::CategorySector(0, 5, 0.0f).end;
    Point(185.0f, boundary, dx, dy);
    CHECK(geometry::HitTest(layout, dx, dy).band == geometry::Band::None);

    Point(135.0f, layout.itemCenter, dx, dy);
    hit = geometry::HitTest(layout, dx, dy);
    CHECK(hit.band == geometry::Band::Items && hit.index == 1);

    Point(135.0f, layout.itemCenter + geometry::kPi, dx, dy);
    CHECK(geometry::HitTest(layout, dx, dy).band == geometry::Band::None);

    Point(105.0f, layout.itemCenter, dx, dy);
    CHECK(geometry::HitTest(layout, dx, dy).band == geometry::Band::None);

    Point(250.0f, geometry::kTop, dx, dy);
    CHECK(geometry::HitTest(layout, dx, dy).band == geometry::Band::None);

    const auto wrapped = SampleLayout(4);
    CHECK(wrapped.itemCenter > geometry::kPi);
    for (int item = 0; item < wrapped.itemCount; ++item) {
        const float middle =
            geometry::ItemSector(item, wrapped.itemCount, wrapped.itemCenter,
                                 wrapped.itemSpan, wrapped.itemMaxSpan,
                                 wrapped.itemGap)
                .Middle();
        Point(135.0f, middle, dx, dy);
        hit = geometry::HitTest(wrapped, dx, dy);
        CHECK(hit.band == geometry::Band::Items && hit.index == item);
    }
}

} // namespace

int main() {
    TestAngles();
    TestSectors();
    TestHitTest();

    if (g_failures != 0) {
        std::cerr << g_failures << " test assertion(s) failed\n";
        return 1;
    }

    std::cout << "wheel_tests: all checks passed\n";
    return 0;
}
