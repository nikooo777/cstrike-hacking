#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "memory/mem.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <limits>
#include <sstream>
#include <string>
#include <tlhelp32.h>

namespace {

bool IsReadableProtection(DWORD protect) {
    if (protect == 0 || (protect & PAGE_GUARD) != 0) {
        return false;
    }

    return (protect & 0xff) != PAGE_NOACCESS;
}

bool IsExecutableProtection(DWORD protect) {
    switch (protect & 0xff) {
    case PAGE_EXECUTE:
    case PAGE_EXECUTE_READ:
    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY:
        return true;
    default:
        return false;
    }
}

bool IsHexDigit(char value) {
    return std::isxdigit(static_cast<unsigned char>(value)) != 0;
}

std::uint8_t HexValue(char value) {
    if (value >= '0' && value <= '9') {
        return static_cast<std::uint8_t>(value - '0');
    }
    if (value >= 'a' && value <= 'f') {
        return static_cast<std::uint8_t>(value - 'a' + 10);
    }
    return static_cast<std::uint8_t>(value - 'A' + 10);
}

} // namespace

char *mem::ScanBasic(const char *pattern, const char *mask, char *begin, intptr_t size) {
    auto matches = ScanBasicAll(pattern, mask, begin, size);
    return matches.empty() ? nullptr : matches.front();
}

std::vector<char *> mem::ScanBasicAll(const char *pattern, const char *mask, char *begin, intptr_t size) {
    std::vector<char *> matches;
    if (pattern == nullptr || mask == nullptr || begin == nullptr || size <= 0) {
        return matches;
    }

    const auto patternLength = std::strlen(mask);
    const auto scanSize = static_cast<std::size_t>(size);
    if (patternLength == 0 || scanSize < patternLength) {
        return matches;
    }

    const auto *bytes = reinterpret_cast<const unsigned char *>(begin);
    const auto *expected = reinterpret_cast<const unsigned char *>(pattern);

    for (std::size_t offset = 0; offset + patternLength <= scanSize; ++offset) {
        bool found = true;
        for (std::size_t index = 0; index < patternLength; ++index) {
            if (mask[index] != '?' && expected[index] != bytes[offset + index]) {
                found = false;
                break;
            }
        }
        if (found) {
            matches.push_back(begin + offset);
        }
    }
    return matches;
}

std::vector<char *> mem::ScanInternalAll(const char *pattern, const char *mask, char *begin, intptr_t size) {
    std::vector<char *> matches;
    if (pattern == nullptr || mask == nullptr || begin == nullptr || size <= 0) {
        return matches;
    }

    const auto beginAddress = reinterpret_cast<std::uintptr_t>(begin);
    const auto requestedSize = static_cast<std::size_t>(size);
    if (requestedSize > std::numeric_limits<std::uintptr_t>::max() - beginAddress) {
        return matches;
    }
    const auto endAddress = beginAddress + requestedSize;

    auto currentAddress = beginAddress;
    while (currentAddress < endAddress) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(reinterpret_cast<const void *>(currentAddress), &mbi, sizeof(mbi)) == 0) {
            break;
        }

        const auto regionBegin = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
        const auto regionSize = static_cast<std::uintptr_t>(mbi.RegionSize);
        if (regionSize == 0 ||
            regionBegin > std::numeric_limits<std::uintptr_t>::max() - regionSize) {
            break;
        }
        const auto regionEnd = regionBegin + regionSize;
        if (regionEnd <= currentAddress) {
            break;
        }

        const auto scanBegin = std::max(currentAddress, beginAddress);
        const auto scanEnd = std::min(regionEnd, endAddress);
        if (scanEnd > scanBegin && mbi.State == MEM_COMMIT && IsReadableProtection(mbi.Protect)) {
            auto regionMatches = ScanBasicAll(pattern, mask, reinterpret_cast<char *>(scanBegin),
                                              static_cast<intptr_t>(scanEnd - scanBegin));
            matches.insert(matches.end(), regionMatches.begin(), regionMatches.end());
        }

        currentAddress = regionEnd;
    }
    return matches;
}

char *mem::ScanInternal(const char *pattern, const char *mask, char *begin, intptr_t size) {
    auto matches = ScanInternalAll(pattern, mask, begin, size);
    return matches.empty() ? nullptr : matches.front();
}

bool mem::Parse(const char *combo, std::vector<std::uint8_t> &pattern,
                std::vector<std::uint8_t> &mask) {
    pattern.clear();
    mask.clear();
    if (combo == nullptr) {
        return false;
    }

    std::istringstream stream(combo);
    std::string token;
    while (stream >> token) {
        if (token == "?" || token == "??" || token == "*" || token == "**") {
            pattern.push_back(0);
            mask.push_back('?');
            continue;
        }

        if (token.size() != 2 || !IsHexDigit(token[0]) || !IsHexDigit(token[1])) {
            pattern.clear();
            mask.clear();
            return false;
        }

        pattern.push_back(static_cast<std::uint8_t>((HexValue(token[0]) << 4) | HexValue(token[1])));
        mask.push_back('x');
    }

    return !pattern.empty() && pattern.size() == mask.size();
}

std::vector<char *> mem::ScanModComboAll(const char *comboPattern, char *begin, intptr_t size) {
    std::vector<char *> matches;
    std::vector<std::uint8_t> pattern;
    std::vector<std::uint8_t> mask;
    if (!Parse(comboPattern, pattern, mask)) {
        return matches;
    }

    // ScanBasicAll treats the mask as a C string when it determines the
    // pattern length. Parse() builds a byte vector, so make that contract
    // explicit before passing the mask across the API boundary.
    mask.push_back('\0');

    return ScanInternalAll(reinterpret_cast<const char *>(pattern.data()),
                           reinterpret_cast<const char *>(mask.data()), begin, size);
}

char *mem::ScanModComboUnique(const char *comboPattern, char *begin, intptr_t size,
                              std::size_t *matchCount) {
    auto matches = ScanModComboAll(comboPattern, begin, size);
    if (matchCount != nullptr) {
        *matchCount = matches.size();
    }
    return matches.size() == 1 ? matches.front() : nullptr;
}

char *mem::ScanModCombo(const char *comboPattern, char *begin, intptr_t size) {
    auto matches = ScanModComboAll(comboPattern, begin, size);
    return matches.empty() ? nullptr : matches.front();
}

bool mem::IsReadable(const void *address, std::size_t size) {
    if (address == nullptr || size == 0) {
        return false;
    }

    const auto beginAddress = reinterpret_cast<std::uintptr_t>(address);
    if (size > std::numeric_limits<std::uintptr_t>::max() - beginAddress) {
        return false;
    }
    const auto endAddress = beginAddress + size;

    auto currentAddress = beginAddress;
    while (currentAddress < endAddress) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(reinterpret_cast<const void *>(currentAddress), &mbi, sizeof(mbi)) == 0) {
            return false;
        }

        const auto regionBegin = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
        const auto regionSize = static_cast<std::uintptr_t>(mbi.RegionSize);
        if (regionSize == 0 ||
            regionBegin > std::numeric_limits<std::uintptr_t>::max() - regionSize) {
            return false;
        }
        const auto regionEnd = regionBegin + regionSize;
        if (regionEnd <= currentAddress ||
            mbi.State != MEM_COMMIT ||
            !IsReadableProtection(mbi.Protect)) {
            return false;
        }
        currentAddress = std::min(regionEnd, endAddress);
    }
    return true;
}

bool mem::IsExecutable(const void *address, std::size_t size) {
    if (address == nullptr || size == 0) {
        return false;
    }

    const auto beginAddress = reinterpret_cast<std::uintptr_t>(address);
    if (size > std::numeric_limits<std::uintptr_t>::max() - beginAddress) {
        return false;
    }
    const auto endAddress = beginAddress + size;

    auto currentAddress = beginAddress;
    while (currentAddress < endAddress) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(reinterpret_cast<const void *>(currentAddress), &mbi, sizeof(mbi)) == 0) {
            return false;
        }

        const auto regionBegin = reinterpret_cast<std::uintptr_t>(mbi.BaseAddress);
        const auto regionSize = static_cast<std::uintptr_t>(mbi.RegionSize);
        if (regionSize == 0 ||
            regionBegin > std::numeric_limits<std::uintptr_t>::max() - regionSize) {
            return false;
        }
        const auto regionEnd = regionBegin + regionSize;
        if (regionEnd <= currentAddress ||
            mbi.State != MEM_COMMIT ||
            !IsExecutableProtection(mbi.Protect)) {
            return false;
        }
        currentAddress = std::min(regionEnd, endAddress);
    }
    return true;
}

bool mem::ReadVirtual(const void *object, std::size_t slot, void *&function) {
    function = nullptr;
    std::uintptr_t vtable = 0;
    if (object == nullptr || !ReadValue(object, vtable) || vtable == 0 ||
        slot > ((std::numeric_limits<std::uintptr_t>::max)() - vtable) /
                   sizeof(void *) ||
        !ReadValue(reinterpret_cast<const void *>(vtable + slot * sizeof(void *)),
                   function) ||
        function == nullptr || !IsExecutable(function)) {
        function = nullptr;
        return false;
    }
    return true;
}

bool mem::ReadBytes(const void *address, void *destination, std::size_t size) {
    // The region check rejects guard pages, which a faulting read would
    // silently disarm. SEH covers memory released after the check.
    if (destination == nullptr || !IsReadable(address, size)) {
        return false;
    }
    __try {
        std::memcpy(destination, address, size);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return true;
}

bool mem::DecodeAbs32(const void *instruction, std::size_t operandOffset,
                      std::uintptr_t &value) {
    if (instruction == nullptr) {
        return false;
    }

    const auto instructionAddress = reinterpret_cast<std::uintptr_t>(instruction);
    if (operandOffset > std::numeric_limits<std::uintptr_t>::max() - instructionAddress) {
        return false;
    }

    std::uint32_t encoded = 0;
    if (!ReadValue(reinterpret_cast<const void *>(instructionAddress + operandOffset), encoded)) {
        return false;
    }
    value = static_cast<std::uintptr_t>(encoded);
    return true;
}

bool mem::DecodeRipRelative32(const void *instruction,
                              std::size_t operandOffset,
                              std::size_t instructionOffset,
                              std::size_t instructionLength,
                              std::uintptr_t &value) {
    if (instruction == nullptr || instructionLength == 0 ||
        operandOffset < instructionOffset ||
        operandOffset - instructionOffset > instructionLength ||
        instructionLength - (operandOffset - instructionOffset) <
            sizeof(std::int32_t)) {
        return false;
    }

    const auto signatureAddress = reinterpret_cast<std::uintptr_t>(instruction);
    if (instructionOffset >
        std::numeric_limits<std::uintptr_t>::max() - signatureAddress) {
        return false;
    }
    const auto instructionAddress = signatureAddress + instructionOffset;
    if (instructionLength >
        std::numeric_limits<std::uintptr_t>::max() - instructionAddress) {
        return false;
    }
    if (operandOffset >
        std::numeric_limits<std::uintptr_t>::max() - signatureAddress) {
        return false;
    }

    std::int32_t displacement = 0;
    if (!ReadValue(reinterpret_cast<const void *>(signatureAddress + operandOffset),
                   displacement)) {
        return false;
    }

    const auto nextInstruction = instructionAddress + instructionLength;
    if (displacement >= 0) {
        const auto positive = static_cast<std::uintptr_t>(displacement);
        if (positive > std::numeric_limits<std::uintptr_t>::max() -
                           nextInstruction) {
            return false;
        }
        value = nextInstruction + positive;
        return true;
    }

    // Avoid negating INT32_MIN in a signed type.
    const auto magnitude = static_cast<std::uintptr_t>(
        -(static_cast<std::int64_t>(displacement)));
    if (magnitude > nextInstruction) {
        return false;
    }
    value = nextInstruction - magnitude;
    return true;
}

std::size_t mem::GetModuleSize(DWORD processID, const char *module) {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processID);
    if (hSnap == INVALID_HANDLE_VALUE) {
        return 0;
    }

    MODULEENTRY32 xModule{};
    xModule.dwSize = sizeof(MODULEENTRY32);
    if (Module32First(hSnap, &xModule)) {
        do {
            if (_stricmp(reinterpret_cast<const char *>(xModule.szModule), module) == 0) {
                CloseHandle(hSnap);
                return static_cast<std::size_t>(xModule.modBaseSize);
            }
        } while (Module32Next(hSnap, &xModule));
    }

    CloseHandle(hSnap);
    return 0;
}

void mem::Patch(BYTE *dst, BYTE *src, unsigned int size) {
    DWORD oldProtect;
    VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, &oldProtect);
    std::memcpy(dst, src, size);
    VirtualProtect(dst, size, oldProtect, &oldProtect);
}
