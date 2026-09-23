#pragma once

#include "math/vector.h"
#include "sdk/render_view.h"

namespace math {

struct ScreenPoint {
    float x = 0.0f;
    float y = 0.0f;
};

bool WorldToScreen(const sdk::render::Matrix4x4 &matrix,
                  const Vector3 &world, float viewportX, float viewportY,
                  float viewportWidth, float viewportHeight,
                  ScreenPoint &screen);

} // namespace math
