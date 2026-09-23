#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "sdk/view_setup.h"

namespace sdk::render {

struct Matrix4x4 {
    float m[4][4];
};

static_assert(sizeof(Matrix4x4) == 0x40);
static_assert(std::is_trivially_copyable_v<Matrix4x4>);

class RenderView {};
class ModelInfo {};

constexpr std::size_t kGetMatricesForViewVtableIndex = 50;
constexpr std::size_t kGetModelVtableIndex = 9;
constexpr std::size_t kGetStudiomodelVtableIndex = 28;
constexpr std::uintptr_t kRenderableSubobjectOffset = 0x8;

#if defined(_M_IX86) || defined(__i386__)
using GetMatricesForViewFn = void(__thiscall *)(
    void *thisPtr, const CViewSetup *view, Matrix4x4 *worldToView,
    Matrix4x4 *viewToProjection, Matrix4x4 *worldToProjection,
    Matrix4x4 *worldToPixels);
using GetModelFn = const void *(__thiscall *)(void *thisPtr);
using GetStudiomodelFn = const void *(__thiscall *)(
    void *thisPtr, const void *model);
#else
using GetMatricesForViewFn = void (*)(
    void *thisPtr, const CViewSetup *view, Matrix4x4 *worldToView,
    Matrix4x4 *viewToProjection, Matrix4x4 *worldToProjection,
    Matrix4x4 *worldToPixels);
using GetModelFn = const void *(*)(void *thisPtr);
using GetStudiomodelFn = const void *(*)(void *thisPtr, const void *model);
#endif

} // namespace sdk::render
