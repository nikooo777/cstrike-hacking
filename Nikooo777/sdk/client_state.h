#pragma once

#include "math/vector.h"
#include "core/padding.h"

class ClientState {
public:
    union {
        DEFINE_MEMBER_N(Vector3, m_vViewAngles, 0x4b84);
    };
};
