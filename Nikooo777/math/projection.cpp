#include "math/projection.h"

#include <cmath>

namespace math {

bool WorldToScreen(const sdk::render::Matrix4x4 &matrix,
                  const Vector3 &world, float viewportX, float viewportY,
                  float viewportWidth, float viewportHeight,
                  ScreenPoint &screen) {
    screen = {};
    if (!std::isfinite(world.x) || !std::isfinite(world.y) ||
        !std::isfinite(world.z) || !std::isfinite(viewportX) ||
        !std::isfinite(viewportY) || !std::isfinite(viewportWidth) ||
        !std::isfinite(viewportHeight) || viewportWidth <= 0.0f ||
        viewportHeight <= 0.0f) {
        return false;
    }

    const float clipX = matrix.m[0][0] * world.x +
                        matrix.m[0][1] * world.y +
                        matrix.m[0][2] * world.z + matrix.m[0][3];
    const float clipY = matrix.m[1][0] * world.x +
                        matrix.m[1][1] * world.y +
                        matrix.m[1][2] * world.z + matrix.m[1][3];
    const float clipW = matrix.m[3][0] * world.x +
                        matrix.m[3][1] * world.y +
                        matrix.m[3][2] * world.z + matrix.m[3][3];
    if (!std::isfinite(clipX) || !std::isfinite(clipY) ||
        !std::isfinite(clipW) || clipW <= 0.001f) {
        return false;
    }

    const float inverseW = 1.0f / clipW;
    const float normalizedX = clipX * inverseW;
    const float normalizedY = clipY * inverseW;
    screen.x = viewportX + (normalizedX + 1.0f) * 0.5f * viewportWidth;
    screen.y = viewportY + (1.0f - normalizedY) * 0.5f * viewportHeight;
    return std::isfinite(screen.x) && std::isfinite(screen.y);
}

} // namespace math
