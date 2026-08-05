#pragma once

#include <cstdint>
#include <string>

namespace core {

// Cached GetModuleHandle bases (client.dll, engine.dll, …).
// uintptr_t so this stays correct if the toolchain is 32- or 64-bit.
std::uintptr_t GetModule(const std::string &module);

} // namespace core
