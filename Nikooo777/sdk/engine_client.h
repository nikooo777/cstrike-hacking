#pragma once

#include "math/vector.h"

// Opaque view of IVEngineClient. We call only the named method below instead
// of reproducing the entire SDK interface, but the slot is checked against
// Source's IVEngineClient declaration and the loaded binary.
class EngineClient {};

// IVEngineClient method order matches Source SDK cdll_int.h for this build:
// GetViewAngles is slot 19 and SetViewAngles is the following slot.
constexpr int kEngineClientGetViewAnglesVtableIndex = 19;
constexpr int kEngineClientSetViewAnglesVtableIndex = 20;

#if defined(_M_IX86) || defined(__i386__)
using EngineClientGetViewAnglesFn = void(__thiscall *)(
    void *thisPtr, Vector3 &angles);
using EngineClientSetViewAnglesFn = void(__thiscall *)(
    void *thisPtr, Vector3 &angles);
#else
using EngineClientGetViewAnglesFn = void (*)(
    void *thisPtr, Vector3 &angles);
using EngineClientSetViewAnglesFn = void (*)(
    void *thisPtr, Vector3 &angles);
#endif
