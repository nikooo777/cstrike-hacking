#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "netvars/netvars.h"

#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "memory/mem.h"

namespace netvars {

namespace {

#if defined(_WIN64) || defined(_M_X64) || defined(__x86_64__)
static_assert(sizeof(void *) == 8, "Expected the x64 netvar metadata ABI");
static_assert(offsetof(ClientClass, recvTable) == 24,
              "Unexpected x64 ClientClass layout");
static_assert(offsetof(RecvProp, offset) == 72,
              "Unexpected x64 RecvProp layout");
static_assert(offsetof(RecvTable, name) == 24,
              "Unexpected x64 RecvTable layout");
#else
static_assert(sizeof(void *) == 4, "Expected the x86 netvar metadata ABI");
static_assert(offsetof(ClientClass, recvTable) == 12,
              "Unexpected x86 ClientClass layout");
static_assert(offsetof(RecvProp, offset) == 44,
              "Unexpected x86 RecvProp layout");
static_assert(offsetof(RecvTable, name) == 12,
              "Unexpected x86 RecvTable layout");
#endif

constexpr std::size_t kMaxClientClasses = 4096;
constexpr int kMaxRecvProps = 8192;
constexpr int kMaxEntityOffset = 0x01000000;

std::map<std::string, const RecvTable *> g_tables;
std::map<std::string, int> g_offsets;
bool g_initialized = false;

std::string MakeKey(const char *tableName, const char *propertyName) {
    return std::string(tableName != nullptr ? tableName : "") + "->" +
           (propertyName != nullptr ? propertyName : "");
}

bool CopyCString(const char *source, std::string &destination, std::size_t maximumLength = 128) {
    destination.clear();
    if (source == nullptr) {
        return false;
    }

    for (std::size_t index = 0; index < maximumLength; ++index) {
        char value = '\0';
        if (!mem::ReadValue(source + index, value)) {
            destination.clear();
            return false;
        }
        if (value == '\0') {
            return true;
        }
        destination.push_back(value);
    }

    destination.clear();
    return false;
}

bool ReadTable(const RecvTable *address, RecvTable &table) {
    if (address == nullptr || !mem::ReadValue(address, table) ||
        table.propCount < 0 || table.propCount > kMaxRecvProps) {
        return false;
    }

    if (table.propCount > 0 && table.props == nullptr) {
        return false;
    }
    if (table.propCount > 0 &&
        !mem::IsReadable(table.props, sizeof(RecvProp) * static_cast<std::size_t>(table.propCount))) {
        return false;
    }
    return true;
}

bool ReadProp(const RecvTable &table, int index, RecvProp &prop) {
    if (index < 0 || index >= table.propCount || table.props == nullptr) {
        return false;
    }
    return mem::ReadValue(table.props + index, prop);
}

bool IsValidOffset(int baseOffset, int propertyOffset, int &combinedOffset) {
    const auto combined = static_cast<std::int64_t>(baseOffset) + propertyOffset;
    if (combined < 0 || combined > kMaxEntityOffset ||
        combined > std::numeric_limits<int>::max()) {
        return false;
    }
    combinedOffset = static_cast<int>(combined);
    return true;
}

bool MatchesProperty(const std::string &actual, const char *requested) {
    if (requested == nullptr) {
        return false;
    }
    if (actual == requested) {
        return true;
    }

    // Source commonly describes a vector/array element as m_name[0] while
    // callers usually ask for the semantic field name m_name.
    const std::string requestedName(requested);
    const std::string firstElement = requestedName + "[0]";
    return actual == firstElement;
}

bool IndexTableGraph(const RecvTable *address,
                     std::map<std::string, const RecvTable *> &tables,
                     std::set<const RecvTable *> &visiting,
                     std::set<const RecvTable *> &visited) {
    if (address == nullptr || visited.count(address) != 0) {
        return true;
    }
    if (visiting.count(address) != 0) {
        return true;
    }

    RecvTable table{};
    if (!ReadTable(address, table)) {
        return false;
    }

    visiting.insert(address);

    std::string tableName;
    if (!CopyCString(table.name, tableName)) {
        visiting.erase(address);
        return false;
    }
    if (!tableName.empty()) {
        tables.emplace(tableName, address);
    }

    for (int index = 0; index < table.propCount; ++index) {
        RecvProp prop{};
        if (!ReadProp(table, index, prop)) {
            visiting.erase(address);
            return false;
        }
        if (prop.dataTable != nullptr &&
            !IndexTableGraph(prop.dataTable, tables, visiting, visited)) {
            visiting.erase(address);
            return false;
        }
    }

    visiting.erase(address);
    visited.insert(address);
    return true;
}

bool BuildTableIndex(ClientClass *classes,
                     std::map<std::string, const RecvTable *> &tables,
                     std::string &error) {
    std::vector<const RecvTable *> roots;
    std::set<const ClientClass *> visitedClasses;

    auto *current = classes;
    for (std::size_t index = 0; current != nullptr && index < kMaxClientClasses; ++index) {
        if (visitedClasses.count(current) != 0) {
            error = "ClientClass list contains a cycle";
            return false;
        }
        visitedClasses.insert(current);

        ClientClass clientClass{};
        if (!mem::ReadValue(current, clientClass)) {
            error = "ClientClass list is not readable";
            return false;
        }
        if (clientClass.recvTable != nullptr) {
            roots.push_back(clientClass.recvTable);
        }
        current = clientClass.next;
    }

    if (current != nullptr) {
        error = "ClientClass list is unexpectedly long";
        return false;
    }
    if (roots.empty()) {
        error = "ClientClass list contains no RecvTables";
        return false;
    }

    std::set<const RecvTable *> visiting;
    std::set<const RecvTable *> visitedTables;
    for (const auto *root : roots) {
        if (!IndexTableGraph(root, tables, visiting, visitedTables)) {
            error = "RecvTable graph is not readable or has an unexpected layout";
            return false;
        }
    }
    if (tables.empty()) {
        error = "RecvTable graph contains no named tables";
        return false;
    }
    return true;
}

bool ResolveProperty(const RecvTable *address,
                     const char *propertyName,
                     int baseOffset,
                     std::set<const RecvTable *> &activeTables,
                     int &result) {
    if (address == nullptr || activeTables.count(address) != 0) {
        return false;
    }

    RecvTable table{};
    if (!ReadTable(address, table)) {
        return false;
    }
    activeTables.insert(address);

    for (int index = 0; index < table.propCount; ++index) {
        RecvProp prop{};
        if (!ReadProp(table, index, prop)) {
            activeTables.erase(address);
            return false;
        }

        std::string actualName;
        if (CopyCString(prop.name, actualName) && MatchesProperty(actualName, propertyName) &&
            IsValidOffset(baseOffset, prop.offset, result)) {
            activeTables.erase(address);
            return true;
        }

        int nestedOffset = 0;
        if (IsValidOffset(baseOffset, prop.offset, nestedOffset) && prop.dataTable != nullptr &&
            ResolveProperty(prop.dataTable, propertyName, nestedOffset, activeTables, result)) {
            activeTables.erase(address);
            return true;
        }
    }

    activeTables.erase(address);
    return false;
}

struct RequiredNetvar {
    const char *table;
    const char *property;
};

constexpr RequiredNetvar kRequiredNetvars[] = {
    {"DT_BasePlayer", "m_lifeState"},
    {"DT_BasePlayer", "m_iHealth"},
    {"DT_BaseEntity", "m_iTeamNum"},
    {"DT_LocalPlayerExclusive", "m_Local"},
    {"DT_LocalPlayerExclusive", "m_vecViewOffset"},
    {"DT_LocalPlayerExclusive", "m_vecVelocity"},
    {"DT_LocalPlayerExclusive", "m_vecBaseVelocity"},
    {"DT_BaseEntity", "m_vecOrigin"},
    {"DT_BaseEntity", "m_angRotation"},
    {"DT_BasePlayer", "m_fFlags"},
    {"DT_Local", "m_vecPunchAngle"},
    {"DT_Local", "m_vecPunchAngleVel"},
    {"DT_CSLocalPlayerExclusive", "m_iShotsFired"},
    {"DT_BaseCombatCharacter", "m_hActiveWeapon"},
    {"DT_WeaponCSBase", "m_weaponMode"},
    {"DT_WeaponCSBase", "m_fAccuracyPenalty"},
};

} // namespace

bool Initialize(ClientClass *classes, std::string &error) {
    g_initialized = false;
    g_tables.clear();
    g_offsets.clear();
    error.clear();

    if (classes == nullptr) {
        error = "GetAllClasses returned null";
        return false;
    }
    if (!BuildTableIndex(classes, g_tables, error)) {
        return false;
    }

    for (const auto &required : kRequiredNetvars) {
        const auto table = g_tables.find(required.table);
        if (table == g_tables.end()) {
            error = "missing RecvTable " + std::string(required.table);
            g_tables.clear();
            return false;
        }

        int offset = -1;
        std::set<const RecvTable *> activeTables;
        if (!ResolveProperty(table->second, required.property, 0, activeTables, offset)) {
            error = "missing netvar " + std::string(required.table) + "->" +
                    required.property;
            g_tables.clear();
            return false;
        }
        g_offsets.emplace(MakeKey(required.table, required.property), offset);
    }

    g_initialized = true;
    return true;
}

bool IsInitialized() {
    return g_initialized;
}

int GetOffset(const char *tableName, const char *propertyName) {
    if (!g_initialized || tableName == nullptr || propertyName == nullptr) {
        return -1;
    }

    const auto key = MakeKey(tableName, propertyName);
    const auto cached = g_offsets.find(key);
    if (cached != g_offsets.end()) {
        return cached->second;
    }

    const auto table = g_tables.find(tableName);
    if (table == g_tables.end()) {
        return -1;
    }

    int offset = -1;
    std::set<const RecvTable *> activeTables;
    if (!ResolveProperty(table->second, propertyName, 0, activeTables, offset)) {
        return -1;
    }
    g_offsets.emplace(key, offset);
    return offset;
}

bool TryGetOffset(const char *tableName, const char *propertyName, int &offset) {
    offset = GetOffset(tableName, propertyName);
    return offset >= 0;
}

void PrintOffsets() {
    if (!g_initialized) {
        std::cout << "Netvars: not initialized" << std::endl;
        return;
    }

    std::cout << "Netvars:" << std::endl;
    for (const auto &entry : g_offsets) {
        std::cout << "  " << entry.first << " = 0x" << std::hex << entry.second
                  << std::dec << std::endl;
    }
}

} // namespace netvars
