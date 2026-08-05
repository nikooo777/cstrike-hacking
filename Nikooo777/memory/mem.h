#pragma once

#include <Windows.h>
#include <cstdint>

namespace mem {

char *ScanBasic(const char *pattern, char *mask, char *begin, intptr_t size);
char *ScanInternal(char *pattern, char *mask, char *begin, intptr_t size);
void Parse(char *combo, char *pattern, char *mask);
char *ScanModCombo(char *comboPattern, char *begin, intptr_t size);
DWORD GetModuleSize(DWORD processID, char *module);
void Patch(BYTE *dst, BYTE *src, unsigned int size);

} // namespace mem
