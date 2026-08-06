#pragma once

#include "math/vector.h"

// Opaque view of IVEngineClient. We call only the named method below instead
// of reproducing the entire SDK interface, but the slot is checked against
// Source's IVEngineClient declaration and the loaded binary.
class EngineClient {};

constexpr int kEngineClientGetViewAnglesVtableIndex = 19;

#if defined(_M_IX86) || defined(__i386__)
using EngineClientGetViewAnglesFn = void(__thiscall *)(
    void *thisPtr, Vector3 &angles);
#else
using EngineClientGetViewAnglesFn = void (*)(
    void *thisPtr, Vector3 &angles);
#endif
