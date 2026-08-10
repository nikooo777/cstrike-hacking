#include "game/interfaces.h"

#include <iostream>
#include <cstddef>
#include <cctype>
#include <cmath>
#include <limits>
#include <string>

#include "core/modules.h"
#include "config/config.h"
#include "memory/mem.h"
#include "sdk/create_interface.h"

namespace game {

namespace {

ClientState *g_clientState = nullptr;
ClientMode *g_clientMode = nullptr;
BaseClient *g_baseClient = nullptr;
IClientEntityList *g_clientEntityList = nullptr;
EngineClient *g_engineClient = nullptr;
sdk::trace::EngineTrace *g_engineTrace = nullptr;
void *g_vguiSurface = nullptr;
std::uintptr_t g_clientStateAddr = 0;

char *FindUniqueSignature(const config::Signature &signature,
                          std::size_t &matchCount) {
    matchCount = 0;
    if (!config::IsLoaded()) {
        std::cout << signature.name << ": config not loaded" << std::endl;
        return nullptr;
    }

    const auto &runtime = config::Get();
    const DWORD pid = GetProcessId(GetCurrentProcess());
    const auto moduleBase = core::GetModule(signature.module.c_str());
    const auto moduleSize = mem::GetModuleSize(pid, signature.module.c_str());
    if (moduleBase == 0 || moduleSize == 0) {
        std::cout << signature.name << ": module unavailable ("
                  << signature.module << ")" << std::endl;
        return nullptr;
    }

    auto *moduleBegin = reinterpret_cast<char *>(moduleBase);
    char *match = nullptr;
    if (runtime.settings.requireUnique) {
        match = mem::ScanModComboUnique(
            signature.pattern.c_str(), moduleBegin,
            static_cast<intptr_t>(moduleSize), &matchCount);
    } else {
        auto matches = mem::ScanModComboAll(
            signature.pattern.c_str(), moduleBegin,
            static_cast<intptr_t>(moduleSize));
        matchCount = matches.size();
        if (!matches.empty()) {
            match = matches.front();
        }
    }

    if (runtime.settings.logMatchOffsets) {
        if (match != nullptr) {
            const auto matchAddress =
                reinterpret_cast<std::uintptr_t>(match);
            std::cout << signature.name << " match: " << signature.module
                      << "+0x" << std::hex << (matchAddress - moduleBase)
                      << std::dec << std::endl;
        }
        std::cout << signature.name << " source: " << signature.source
                  << std::endl;
        if (!signature.sourceReadme.empty()) {
            std::cout << signature.name << " README: "
                      << signature.sourceReadme << std::endl;
        }
        if (!signature.sourceVideo.empty()) {
            std::cout << signature.name << " video: "
                      << signature.sourceVideo << std::endl;
        }
        std::cout << signature.name << " discovery: "
                  << signature.discovery << std::endl;
    }

    if (match == nullptr) {
        if (matchCount == 0) {
            std::cout << signature.name << " signature not found" << std::endl;
        } else if (runtime.settings.requireUnique) {
            std::cout << signature.name << " signature is ambiguous ("
                      << matchCount << " matches)" << std::endl;
        }
    } else if (!runtime.settings.requireUnique && matchCount > 1) {
        std::cout << signature.name << " is using the first of "
                  << matchCount << " matches because require_unique=false"
                  << std::endl;
    }

    return match;
}

bool HasUsableVtableSlot(const void *instance, std::size_t slot) {
    auto *vtable = mem::ReadPointer<void>(instance);
    if (vtable == nullptr || !mem::IsReadable(vtable, sizeof(void *))) {
        return false;
    }

    const auto vtableAddress = reinterpret_cast<std::uintptr_t>(vtable);
    if (slot > ((std::numeric_limits<std::uintptr_t>::max)() -
                vtableAddress) / sizeof(void *)) {
        return false;
    }

    const auto slotAddress = vtableAddress + slot * sizeof(void *);
    if (!mem::IsReadable(reinterpret_cast<const void *>(slotAddress),
                         sizeof(void *))) {
        return false;
    }

    void *function = nullptr;
    if (!mem::ReadValue(reinterpret_cast<const void *>(slotAddress),
                        function)) {
        return false;
    }
    return function != nullptr && mem::IsExecutable(function);
}

bool HasUsableVtable(const void *instance) {
    return HasUsableVtableSlot(instance, 0);
}

bool DecodeConfiguredOperand(const char *name, char *scan,
                             const config::Signature &signature,
                             std::uintptr_t &value) {
    std::string operand = signature.operand;
    for (char &character : operand) {
        character = static_cast<char>(
            std::tolower(static_cast<unsigned char>(character)));
    }

    if (operand == "abs32") {
        return mem::DecodeAbs32(scan, signature.operandOffset, value);
    }
    if (operand == "rip_rel32") {
        return mem::DecodeRipRelative32(
            scan, signature.operandOffset, signature.instructionOffset,
            signature.instructionLength, value);
    }
    if (operand == "match") {
        value = reinterpret_cast<std::uintptr_t>(scan);
        return true;
    }

    std::cout << name << " has unsupported operand type: "
              << signature.operand << std::endl;
    return false;
}

} // namespace

char *FindConfiguredSignature(const config::Signature &signature,
                              std::size_t &matchCount) {
    return FindUniqueSignature(signature, matchCount);
}

ClientState *GetClientState() {
    if (g_clientState) {
        return g_clientState;
    }

    std::size_t matchCount = 0;
    const auto &signature = config::Get().clientState;
    auto *scan = FindUniqueSignature(signature, matchCount);
    if (scan == nullptr) {
        return nullptr;
    }

    if (signature.indirections != 0) {
        std::cout << "ClientState signature has an unsupported pointer chain"
                  << std::endl;
        return nullptr;
    }

    std::uintptr_t stateAddress = 0;
    if (!DecodeConfiguredOperand("ClientState", scan, signature,
                                stateAddress) ||
        stateAddress == 0) {
        std::cout << "ClientState operand could not be decoded" << std::endl;
        return nullptr;
    }
    if (config::Get().settings.logMatchOffsets) {
        std::cout << "ClientState decoded target: 0x" << std::hex
                  << stateAddress << std::dec << std::endl;
    }

    auto *clientState = reinterpret_cast<ClientState *>(stateAddress);
    if (config::Get().settings.validatePointers &&
        !mem::IsReadable(clientState, sizeof(ClientState))) {
        std::cout << "ClientState pointer is not readable" << std::endl;
        return nullptr;
    }

    g_clientStateAddr = stateAddress;
    g_clientState = clientState;
    return clientState;
}

ClientMode *GetClientMode() {
    if (g_clientMode) {
        return g_clientMode;
    }

    std::size_t matchCount = 0;
    const auto &signature = config::Get().clientMode;
    auto *scan = FindUniqueSignature(signature, matchCount);
    if (scan == nullptr) {
        return nullptr;
    }
    // Pattern points at `mov ecx, [g_pClientMode]` — +2 skips opcode/modrm to the absolute address.
    // Decode 8B 0D imm32 as a global-slot address, then read ClientMode* from that slot.
    std::uintptr_t clientModeAddress = 0;
    if (!DecodeConfiguredOperand("ClientMode", scan, signature,
                                clientModeAddress) ||
        clientModeAddress == 0) {
        std::cout << "ClientMode operand could not be decoded" << std::endl;
        return nullptr;
    }

    ClientMode *clientMode = nullptr;
    if (signature.indirections == 0) {
        clientMode = reinterpret_cast<ClientMode *>(clientModeAddress);
    } else if (signature.indirections == 1) {
        clientMode = mem::ReadPointer<ClientMode>(
            reinterpret_cast<const void *>(clientModeAddress));
    } else {
        std::cout << "ClientMode signature has unsupported pointer depth"
                  << std::endl;
        return nullptr;
    }
    if (clientMode == nullptr ||
        (config::Get().settings.validatePointers &&
         signature.validateVtable && !HasUsableVtable(clientMode))) {
        std::cout << "ClientMode pointer or vtable is not usable" << std::endl;
        return nullptr;
    }

    g_clientMode = clientMode;
    std::cout << "ClientMode: 0x" << std::hex << g_clientMode << std::endl;
    return g_clientMode;
}

BaseClient *GetBaseClient() {
    if (g_baseClient) {
        return g_baseClient;
    }
    g_baseClient = static_cast<BaseClient *>(GetInterface("client.dll", "VClient017"));
    return g_baseClient;
}

IClientEntityList *GetClientEntityList() {
    if (g_clientEntityList != nullptr) {
        return g_clientEntityList;
    }
    if (!config::IsLoaded()) {
        std::cout << "ClientEntityList: config not loaded" << std::endl;
        return nullptr;
    }

    const auto &definition = config::Get().clientEntityList;
    g_clientEntityList = static_cast<IClientEntityList *>(
        GetInterface(definition.module.c_str(), definition.name.c_str()));
    if (g_clientEntityList == nullptr) {
        std::cout << "ClientEntityList interface not found: "
                  << definition.name << std::endl;
        return nullptr;
    }

    if (config::Get().settings.validatePointers &&
        !HasUsableVtable(g_clientEntityList)) {
        std::cout << "ClientEntityList vtable is not usable" << std::endl;
        g_clientEntityList = nullptr;
        return nullptr;
    }

    if (config::Get().settings.logMatchOffsets) {
        std::cout << "ClientEntityList source: " << definition.source
                  << std::endl;
        if (!definition.sourceReadme.empty()) {
            std::cout << "ClientEntityList README: "
                      << definition.sourceReadme << std::endl;
        }
        std::cout << "ClientEntityList discovery: "
                  << definition.discovery << std::endl;
    }
    std::cout << "ClientEntityList: " << definition.name << " at 0x"
              << std::hex << g_clientEntityList << std::dec << std::endl;
    return g_clientEntityList;
}

EngineClient *GetEngineClient() {
    if (g_engineClient != nullptr) {
        return g_engineClient;
    }
    if (!config::IsLoaded()) {
        std::cout << "EngineClient: config not loaded" << std::endl;
        return nullptr;
    }

    const auto &definition = config::Get().engineClient;
    g_engineClient = static_cast<EngineClient *>(
        GetInterface(definition.module.c_str(), definition.name.c_str()));
    if (g_engineClient == nullptr) {
        std::cout << "EngineClient interface not found: "
                  << definition.name << std::endl;
        return nullptr;
    }

    if (config::Get().settings.validatePointers &&
        (!HasUsableVtableSlot(
             g_engineClient, kEngineClientGetViewAnglesVtableIndex) ||
         !HasUsableVtableSlot(
             g_engineClient, kEngineClientSetViewAnglesVtableIndex))) {
        std::cout << "EngineClient Get/SetViewAngles vtable slots are not usable"
                  << std::endl;
        g_engineClient = nullptr;
        return nullptr;
    }

    if (config::Get().settings.logMatchOffsets) {
        std::cout << "EngineClient source: " << definition.source
                  << std::endl;
        if (!definition.sourceReadme.empty()) {
            std::cout << "EngineClient README: "
                      << definition.sourceReadme << std::endl;
        }
        std::cout << "EngineClient discovery: "
                  << definition.discovery << std::endl;
    }
    std::cout << "EngineClient: " << definition.name << " at 0x"
              << std::hex << g_engineClient << std::dec << std::endl;
    return g_engineClient;
}

sdk::trace::EngineTrace *GetEngineTrace() {
    if (g_engineTrace != nullptr) {
        return g_engineTrace;
    }
    if (!config::IsLoaded()) {
        std::cout << "EngineTrace: config not loaded" << std::endl;
        return nullptr;
    }

    const auto &definition = config::Get().engineTrace;
    g_engineTrace = static_cast<sdk::trace::EngineTrace *>(
        GetInterface(definition.module.c_str(), definition.name.c_str()));
    if (g_engineTrace == nullptr) {
        std::cout << "EngineTrace interface not found: "
                  << definition.name << std::endl;
        return nullptr;
    }

    if (config::Get().settings.validatePointers &&
        !HasUsableVtableSlot(g_engineTrace,
                             sdk::trace::kTraceRayVtableIndex)) {
        std::cout << "EngineTrace TraceRay vtable slot is not usable"
                  << std::endl;
        g_engineTrace = nullptr;
        return nullptr;
    }

    if (config::Get().settings.logMatchOffsets) {
        std::cout << "EngineTrace source: " << definition.source
                  << std::endl;
        if (!definition.sourceReadme.empty()) {
            std::cout << "EngineTrace README: "
                      << definition.sourceReadme << std::endl;
        }
        std::cout << "EngineTrace discovery: "
                  << definition.discovery << std::endl;
    }
    std::cout << "EngineTrace: " << definition.name << " at 0x"
              << std::hex << g_engineTrace << std::dec << std::endl;
    return g_engineTrace;
}

void *GetVguiSurface() {
    if (g_vguiSurface == nullptr) {
        g_vguiSurface =
            GetInterface("vguimatsurface.dll", "VGUI_Surface030");
    }
    return g_vguiSurface;
}

namespace {

bool CallEngineViewAngles(Vector3 &angles, int slotIndex) {
    auto *engineClient = GetEngineClient();
    if (engineClient == nullptr) {
        return false;
    }

    auto *vtable = mem::ReadPointer<void>(engineClient);
    if (vtable == nullptr) {
        return false;
    }

    const auto slot = static_cast<std::size_t>(slotIndex);
    const auto vtableAddress = reinterpret_cast<std::uintptr_t>(vtable);
    if (slot > ((std::numeric_limits<std::uintptr_t>::max)() -
                vtableAddress) / sizeof(void *)) {
        return false;
    }

    void *method = nullptr;
    const auto methodAddress = vtableAddress + slot * sizeof(void *);
    if (!mem::ReadValue(reinterpret_cast<const void *>(methodAddress),
                        method) ||
        method == nullptr || !mem::IsExecutable(method)) {
        return false;
    }

    if (slotIndex == kEngineClientGetViewAnglesVtableIndex) {
        __try {
            reinterpret_cast<EngineClientGetViewAnglesFn>(method)(engineClient,
                                                                  angles);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
        return true;
    }
    if (slotIndex == kEngineClientSetViewAnglesVtableIndex) {
        __try {
            reinterpret_cast<EngineClientSetViewAnglesFn>(method)(engineClient,
                                                                  angles);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
        return true;
    }
    return false;
}

} // namespace

bool GetViewAngles(Vector3 &angles) {
    return CallEngineViewAngles(angles, kEngineClientGetViewAnglesVtableIndex);
}

bool SetViewAngles(Vector3 &angles) {
    return CallEngineViewAngles(angles, kEngineClientSetViewAnglesVtableIndex);
}

bool TraceLine(const Vector3 &start, const Vector3 &end,
               const void *skipFirst, const void *skipSecond, float &fraction) {
    fraction = 0.0f;
    if (!std::isfinite(start.x) || !std::isfinite(start.y) ||
        !std::isfinite(start.z) || !std::isfinite(end.x) ||
        !std::isfinite(end.y) || !std::isfinite(end.z)) {
        return false;
    }

    auto *engineTrace = GetEngineTrace();
    if (engineTrace == nullptr) {
        return false;
    }

    auto *vtable = mem::ReadPointer<void>(engineTrace);
    if (vtable == nullptr) {
        return false;
    }

    const auto slot = sdk::trace::kTraceRayVtableIndex;
    const auto vtableAddress = reinterpret_cast<std::uintptr_t>(vtable);
    if (slot > ((std::numeric_limits<std::uintptr_t>::max)() -
                vtableAddress) /
                   sizeof(void *)) {
        return false;
    }

    void *method = nullptr;
    const auto methodAddress = vtableAddress + slot * sizeof(void *);
    if (!mem::ReadValue(reinterpret_cast<const void *>(methodAddress), method) ||
        method == nullptr || !mem::IsExecutable(method)) {
        return false;
    }

    sdk::trace::Ray ray;
    ray.Init(start, end);
    sdk::trace::TraceFilterSkipEntities filter(skipFirst, skipSecond);
    sdk::trace::GameTrace trace{};

    bool called = false;
    __try {
        reinterpret_cast<sdk::trace::TraceRayFn>(method)(
            engineTrace, ray, sdk::trace::kMaskVisible, &filter, &trace);
        called = true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }

    if (!called || !std::isfinite(trace.fraction) || trace.fraction < 0.0f ||
        trace.fraction > 1.0f) {
        return false;
    }

    fraction = trace.fraction;
    return true;
}

std::uintptr_t GetClientStateAddress() {
    if (!g_clientState) {
        GetClientState();
    }
    return g_clientStateAddr;
}

} // namespace game
