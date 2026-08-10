#pragma once

#include "math/vector.h"

class CCSPlayer;

namespace features {

struct RecoilState {
    Vector3 punchAngles{};
    bool punchReadable = false;
};

// Read only the live values needed by the shared shot-angle pipeline. The
// command is not mutated here; composition belongs to game::ComposeShotAngles.
bool ReadRecoilState(const CCSPlayer *player, RecoilState &state);

}
