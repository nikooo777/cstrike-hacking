#pragma once

#include "core/arch.h"
#include "math/vector.h"

// Opaque view of IVEngineClient. We call only the named method below instead
// of reproducing the entire SDK interface, but the slot is checked against
// Source's IVEngineClient declaration and the loaded binary.
class EngineClient {};

// IVEngineClient method order matches Source SDK cdll_int.h for this build:
// GetViewAngles is slot 19 and SetViewAngles is the following slot.
constexpr int kEngineClientGetViewAnglesVtableIndex = 19;
constexpr int kEngineClientSetViewAnglesVtableIndex = 20;

using EngineClientGetViewAnglesFn = void(ARCH_THISCALL *)(void *thisPtr,
                                                         Vector3 &angles);
using EngineClientSetViewAnglesFn = void(ARCH_THISCALL *)(void *thisPtr,
                                                         Vector3 &angles);
