#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace netvars {

// These declarations mirror the Source 1 client-side metadata ABI. Pointer
// fields intentionally use pointer types so the same traversal works on x86
// and x64; netvars.cpp asserts the concrete offsets for each ABI.
struct RecvTable;

struct ClientClass {
    void *createFn;
    void *createEventFn;
    const char *networkName;
    RecvTable *recvTable;
    ClientClass *next;
    int classId;
};

struct RecvProp {
    const char *name;
    int recvType;
    int flags;
    int stringBufferSize;
    bool insideArray;
    const void *extraData;
    RecvProp *arrayProp;
    void *arrayLengthProxy;
    void *proxyFn;
    void *dataTableProxyFn;
    RecvTable *dataTable;
    int offset;
    int elementStride;
    int elementCount;
    const char *parentArrayPropName;
};

struct RecvTable {
    RecvProp *props;
    int propCount;
    void *decoder;
    const char *name;
    bool initialized;
    bool inMainList;
};

bool Initialize(ClientClass *classes, std::string &error);
bool IsInitialized();

// Returns an entity-relative offset, or -1 when the table/property is absent.
// The resolver recursively follows data tables and accepts an exact property
// name or the common Source array spelling, e.g. m_vecVelocity[0].
int GetOffset(const char *tableName, const char *propertyName);
bool TryGetOffset(const char *tableName, const char *propertyName, int &offset);

void PrintOffsets();

} // namespace netvars

// Netvar accessors deliberately look up their displacement at runtime. The
// caller must initialize netvars before accessing a field through one of
// these methods; hooks::MainThread does that before enabling feature hooks.
#define DEFINE_NETVAR(type, name, tableName, propertyName) \
    type &name() { \
        static const int offset = netvars::GetOffset(tableName, propertyName); \
        return *reinterpret_cast<type *>(reinterpret_cast<std::uintptr_t>(this) + offset); \
    } \
    const type &name() const { \
        static const int offset = netvars::GetOffset(tableName, propertyName); \
        return *reinterpret_cast<const type *>(reinterpret_cast<std::uintptr_t>(this) + offset); \
    }
