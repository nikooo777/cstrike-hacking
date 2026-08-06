#pragma once

#include <cstdint>
#include <type_traits>

// Source declares handle-taking virtuals with CBaseHandle by value. This is
// deliberately a non-trivial 4-byte class: on MSVC x64, replacing it with a
// uint32_t can change the parameter ABI because the compiler may pass a
// temporary object pointer for the class form.
class CBaseHandle {
public:
    CBaseHandle() : m_index(0xFFFFFFFFu) {}
    explicit CBaseHandle(std::uint32_t value) : m_index(value) {}
    CBaseHandle(const CBaseHandle &other) : m_index(other.m_index) {}

    CBaseHandle &operator=(const CBaseHandle &other) {
        m_index = other.m_index;
        return *this;
    }

    std::uint32_t ToInt() const { return m_index; }

private:
    std::uint32_t m_index;
};

static_assert(sizeof(CBaseHandle) == sizeof(std::uint32_t),
              "CBaseHandle must remain a 32-bit handle value");
static_assert(!std::is_trivially_copyable_v<CBaseHandle>,
              "CBaseHandle ABI must remain non-trivial on MSVC x64");

// Source 1 IClientEntityList vtable. The first three methods are kept here so
// GetClientEntity remains at the ABI-correct slot 3 on x86 and x64.
class IClientEntityList {
public:
    virtual void *GetClientNetworkable(int entnum) = 0;
    virtual void *GetClientNetworkableFromHandle(CBaseHandle handle) = 0;
    virtual void *GetClientUnknownFromHandle(CBaseHandle handle) = 0;
    virtual void *GetClientEntity(int entnum) = 0;
    virtual void *GetClientEntityFromHandle(CBaseHandle handle) = 0;
    virtual int NumberOfEntities(bool includeNonNetworkable) = 0;
    virtual int GetHighestEntityIndex() = 0;
    virtual void SetMaxEntities(int maxEntities) = 0;
    virtual int GetMaxEntities() = 0;
};
