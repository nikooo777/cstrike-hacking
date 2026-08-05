#pragma once

#include <cstdint>

#define STR_MERGE_IMPL(a, b) a##b
#define STR_MERGE(a, b) STR_MERGE_IMPL(a, b)
#define MAKE_PAD(size) STR_MERGE(_pad, __COUNTER__)[size]

// Overlay member via in-class padding. Offsets are relative to the start of the
// enclosing union/struct — do NOT use on a derived class for absolute entity offsets.
#define DEFINE_MEMBER_N(type, name, offset) struct { unsigned char MAKE_PAD(offset); type name; }

// Absolute offset from `this`. Safe with C++ inheritance (CBasePlayer / CCSPlayer).
// Usage: player->m_iHealth()  /  player->m_Local().m_vecPunchAngle()
#define DEFINE_MEMBER(type, name, offset) \
    type &name() { return *reinterpret_cast<type *>(reinterpret_cast<std::uintptr_t>(this) + (offset)); } \
    const type &name() const { return *reinterpret_cast<const type *>(reinterpret_cast<std::uintptr_t>(this) + (offset)); }
