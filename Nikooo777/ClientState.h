#pragma once

#include "vector.h"
#include "padding.h"

class ClientState {
public:
    union {
        //              Type     Name    Offset
        DEFINE_MEMBER_N(Vector3, m_vViewAngles, 0x4b84);
    };
};
