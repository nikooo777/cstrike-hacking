#include "game/interfaces.h"

#include <iostream>
#include <cstddef>

#include "core/modules.h"
#include "config/config.h"
#include "memory/mem.h"
#include "sdk/create_interface.h"

namespace game {

namespace {

ClientState *g_clientState = nullptr;
ClientMode *g_clientMode = nullptr;
BaseClient *g_baseClient = nullptr;
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
    const DWORD moduleSize = mem::GetModuleSize(pid, signature.module.c_str());
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

bool HasUsableVtable(const void *instance) {
    auto *vtable = mem::ReadPointer<void>(instance);
    if (vtable == nullptr || !mem::IsReadable(vtable, sizeof(void *))) {
        return false;
    }

    auto *firstFunction = mem::ReadPointer<void>(vtable);
    return firstFunction != nullptr && mem::IsExecutable(firstFunction);
}

} // namespace

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
    if (!mem::DecodeAbs32(scan, signature.operandOffset, stateAddress) ||
        stateAddress == 0) {
        std::cout << "ClientState operand could not be decoded" << std::endl;
        return nullptr;
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
    if (signature.indirections != 1) {
        std::cout << "ClientMode signature must describe one pointer indirection"
                  << std::endl;
        return nullptr;
    }

    std::uintptr_t globalSlot = 0;
    if (!mem::DecodeAbs32(scan, signature.operandOffset, globalSlot) ||
        globalSlot == 0) {
        std::cout << "ClientMode operand could not be decoded" << std::endl;
        return nullptr;
    }

    auto *clientMode =
        mem::ReadPointer<ClientMode>(reinterpret_cast<const void *>(globalSlot));
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

std::uintptr_t GetClientStateAddress() {
    if (!g_clientState) {
        GetClientState();
    }
    return g_clientStateAddr;
}

} // namespace game
