#pragma once

#include <cstdint>

// Source 1 IClientEntityList vtable. The first three methods are kept here so
// GetClientEntity remains at the ABI-correct slot 3 on x86 and x64.
class IClientEntityList {
public:
    virtual void *GetClientNetworkable(int entnum) = 0;
    virtual void *GetClientNetworkableFromHandle(std::uint32_t handle) = 0;
    virtual void *GetClientUnknownFromHandle(std::uint32_t handle) = 0;
    virtual void *GetClientEntity(int entnum) = 0;
    virtual void *GetClientEntityFromHandle(std::uint32_t handle) = 0;
    virtual int NumberOfEntities(bool includeNonNetworkable) = 0;
    virtual int GetHighestEntityIndex() = 0;
    virtual void SetMaxEntities(int maxEntities) = 0;
    virtual int GetMaxEntities() = 0;
};
