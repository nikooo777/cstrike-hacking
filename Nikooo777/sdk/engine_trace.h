#pragma once

#include <cstddef>
#include <cstdint>

#include "core/arch.h"
#include "math/vector.h"

namespace sdk::trace {

// Source SDK IEngineTrace::TraceRay is the fifth virtual method, counted from
// zero. Keep this ABI declaration minimal: the feature only needs a line ray,
// a two-entity filter, and the trace fraction.
constexpr std::size_t kTraceRayVtableIndex = 4;
constexpr unsigned int kMaskVisible =
    0x00000001u | // CONTENTS_SOLID
    0x00004000u | // CONTENTS_MOVEABLE
    0x00000080u | // CONTENTS_OPAQUE
    0x00002000u;  // CONTENTS_IGNORE_NODRAW_OPAQUE

enum class TraceType : int {
    Everything = 0,
    WorldOnly = 1,
    EntitiesOnly = 2,
    EverythingFilterProps = 3,
};

struct alignas(16) VectorAligned {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;

    void Set(const Vector3 &value) {
        x = value.x;
        y = value.y;
        z = value.z;
        w = 0.0f;
    }
};

static_assert(sizeof(VectorAligned) == 0x10);

struct Ray {
    VectorAligned m_Start{};
    VectorAligned m_Delta{};
    VectorAligned m_StartOffset{};
    VectorAligned m_Extents{};
    bool m_IsRay = false;
    bool m_IsSwept = false;

    void Init(const Vector3 &start, const Vector3 &end) {
        m_Delta.x = end.x - start.x;
        m_Delta.y = end.y - start.y;
        m_Delta.z = end.z - start.z;
        m_Delta.w = 0.0f;

        m_IsSwept = (m_Delta.x != 0.0f || m_Delta.y != 0.0f ||
                     m_Delta.z != 0.0f);

        m_Extents = {};
        m_IsRay = true;

        m_StartOffset = {};
        m_Start.Set(start);
    }
};

static_assert(offsetof(Ray, m_IsRay) == 0x40);
static_assert(offsetof(Ray, m_IsSwept) == 0x41);
static_assert(sizeof(Ray) == 0x50);

struct TracePlane {
    float normalX = 0.0f;
    float normalY = 0.0f;
    float normalZ = 0.0f;
    float distance = 0.0f;
    std::uint8_t type = 0;
    std::uint8_t signBits = 0;
    std::uint8_t pad0 = 0;
    std::uint8_t pad1 = 0;
};

static_assert(sizeof(TracePlane) == 0x14);

struct TraceSurface {
    const char *name = nullptr;
    std::int16_t surfaceProperties = 0;
    std::uint16_t flags = 0;
};

static_assert(sizeof(TraceSurface) == (sizeof(void *) == 8 ? 0x10 : 0x08));

// CGameTrace-compatible prefix and tail. TraceRay writes more than fraction,
// so the full layout is represented even though visibility only consumes the
// fraction field.
struct GameTrace {
    Vector3 startPosition{};
    Vector3 endPosition{};
    TracePlane plane{};
    float fraction = 0.0f;
    int contents = 0;
    std::uint16_t displacementFlags = 0;
    bool allSolid = false;
    bool startSolid = false;
    float fractionLeftSolid = 0.0f;
    TraceSurface surface{};
    int hitGroup = 0;
    std::int16_t physicsBone = 0;
    void *entity = nullptr;
    int hitBox = 0;
};

static_assert(offsetof(GameTrace, fraction) == 0x2C);
static_assert(offsetof(GameTrace, entity) == (sizeof(void *) == 8 ? 0x58 : 0x4C));
static_assert(sizeof(GameTrace) == (sizeof(void *) == 8 ? 0x68 : 0x54));

class ITraceFilter {
public:
    virtual bool ShouldHitEntity(void *entity, int contentsMask) = 0;
    virtual TraceType GetTraceType() const = 0;
};

class TraceFilterSkipEntities final : public ITraceFilter {
public:
    TraceFilterSkipEntities(const void *first, const void *second)
        : first_(first), second_(second) {}

    bool ShouldHitEntity(void *entity, int) override {
        return entity != first_ && entity != second_;
    }

    TraceType GetTraceType() const override {
        return TraceType::Everything;
    }

private:
    const void *first_ = nullptr;
    const void *second_ = nullptr;
};

class EngineTrace {};

using TraceRayFn = void(ARCH_THISCALL *)(void *thisPtr, const Ray &ray,
                                         unsigned int mask,
                                         ITraceFilter *filter,
                                         GameTrace *trace);

} // namespace sdk::trace
