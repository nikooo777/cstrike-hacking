#pragma once

#include <Windows.h>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace mem {

char *ScanBasic(const char *pattern, const char *mask, char *begin, intptr_t size);
std::vector<char *> ScanBasicAll(const char *pattern, const char *mask, char *begin, intptr_t size);
char *ScanInternal(const char *pattern, const char *mask, char *begin, intptr_t size);
std::vector<char *> ScanInternalAll(const char *pattern, const char *mask, char *begin, intptr_t size);
bool Parse(const char *combo, std::vector<std::uint8_t> &pattern, std::vector<std::uint8_t> &mask);
std::vector<char *> ScanModComboAll(const char *comboPattern, char *begin, intptr_t size);
char *ScanModComboUnique(const char *comboPattern, char *begin, intptr_t size,
                         std::size_t *matchCount = nullptr);
char *ScanModCombo(const char *comboPattern, char *begin, intptr_t size);
std::size_t GetModuleSize(DWORD processID, const char *module);

bool IsReadable(const void *address, std::size_t size);
bool IsExecutable(const void *address, std::size_t size = 1);
bool ReadBytes(const void *address, void *destination, std::size_t size);
bool DecodeAbs32(const void *instruction, std::size_t operandOffset, std::uintptr_t &value);
bool DecodeRipRelative32(const void *instruction, std::size_t operandOffset,
                         std::size_t instructionOffset,
                         std::size_t instructionLength,
                         std::uintptr_t &value);

template <typename T>
bool ReadValue(const void *address, T &value) {
    static_assert(std::is_trivially_copyable_v<T>, "ReadValue requires a trivially copyable type");
    return ReadBytes(address, &value, sizeof(T));
}

template <typename T>
T *ReadPointer(const void *address) {
    T *value = nullptr;
    return ReadValue(address, value) ? value : nullptr;
}

void Patch(BYTE *dst, BYTE *src, unsigned int size);

} // namespace mem
