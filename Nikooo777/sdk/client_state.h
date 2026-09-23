#pragma once

#include "core/arch.h"
#include "math/vector.h"
#include "core/padding.h"

class ClientState {
public:
#if ARCH_X86()
    union {
        DEFINE_MEMBER_N(Vector3, m_vViewAngles, 0x4b84);
    };
#endif
};
