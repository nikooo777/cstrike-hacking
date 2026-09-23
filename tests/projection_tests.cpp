#include <cassert>

#include "math/projection.h"

int main() {
    sdk::render::Matrix4x4 identity{};
    identity.m[0][0] = 1.0f;
    identity.m[1][1] = 1.0f;
    identity.m[2][2] = 1.0f;
    identity.m[3][3] = 1.0f;

    math::ScreenPoint screen{};
    assert(math::WorldToScreen(identity, Vector3{0.0f, 0.0f, 0.0f},
                               0.0f, 0.0f, 800.0f, 600.0f, screen));
    assert(screen.x == 400.0f);
    assert(screen.y == 300.0f);

    assert(math::WorldToScreen(identity, Vector3{1.0f, 1.0f, 0.0f},
                               0.0f, 0.0f, 800.0f, 600.0f, screen));
    assert(screen.x == 800.0f);
    assert(screen.y == 0.0f);

    assert(!math::WorldToScreen(identity, Vector3{0.0f, 0.0f, 0.0f},
                                0.0f, 0.0f, 0.0f, 600.0f, screen));

    sdk::render::Matrix4x4 behind = identity;
    behind.m[3][3] = -1.0f;
    assert(!math::WorldToScreen(behind, Vector3{0.0f, 0.0f, 0.0f},
                                0.0f, 0.0f, 800.0f, 600.0f, screen));
    return 0;
}
