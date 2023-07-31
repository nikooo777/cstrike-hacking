//
// Created by Niko on 7/12/2021.
//

#pragma once

namespace mem {
    char *ScanBasic(const char *pattern, char *mask, char *begin, intptr_t size);

    char *ScanInternal(char *pattern, char *mask, char *begin, intptr_t size);

    void Parse(char *combo, char *pattern, char *mask);

    char *ScanModCombo(char *comboPattern, char *begin, intptr_t size);

    DWORD GetModuleSize(DWORD processID, char *module);

    void Patch(BYTE *dst, BYTE *src, unsigned int size);
}
