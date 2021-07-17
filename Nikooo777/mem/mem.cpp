//
// Created by Niko on 7/12/2021.
//
#include <Windows.h>
#include <tlhelp32.h>
#include "mem.h"

char *ScanBasic(char *pattern, char *mask, char *begin, intptr_t size) {
    intptr_t patternLen = strlen(mask);

    for (int i = 0; i < size; i++) {
        bool found = true;
        for (int j = 0; j < patternLen; j++) {
            if (mask[j] != '?' && pattern[j] != *(char *) ((intptr_t) begin + i + j)) {
                found = false;
                break;
            }
        }
        if (found) {
            return (begin + i);
        }
    }
    return nullptr;
}

char *ScanInternal(char *pattern, char *mask, char *begin, intptr_t size) {
    char *match{nullptr};
    MEMORY_BASIC_INFORMATION mbi{};

    for (char *curr = begin; curr < begin + size; curr += mbi.RegionSize) {
        if (!VirtualQuery(curr, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT || mbi.Protect == PAGE_NOACCESS) continue;

        match = ScanBasic(pattern, mask, curr, mbi.RegionSize);

        if (match != nullptr) {
            break;
        }
    }
    return match;
}

void Parse(char *combo, char *pattern, char *mask) {
    char lastChar = ' ';
    unsigned int j = 0;

    for (unsigned int i = 0; i < strlen(combo); i++) {
        if ((combo[i] == '?' || combo[i] == '*') && (lastChar != '?' && lastChar != '*')) {
            pattern[j] = mask[j] = '?';
            j++;
        } else if (isspace(lastChar)) {
            pattern[j] = lastChar = (char) strtol(&combo[i], nullptr, 16);
            mask[j] = 'x';
            j++;
        }
        lastChar = combo[i];
    }
    pattern[j] = mask[j] = '\0';
}
char* ScanModCombo(char* comboPattern, char *begin, intptr_t size)
{
    char pattern[100]{};
    char mask[100]{};
    Parse(comboPattern, pattern, mask);
    return ScanInternal(pattern, mask,begin,size);
}

DWORD GetModuleSize(DWORD processID, char* module)
{
    HANDLE hSnap;
    MODULEENTRY32 xModule;
    hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processID);
    xModule.dwSize = sizeof(MODULEENTRY32);
    if (Module32First(hSnap, &xModule)) {
        while (Module32Next(hSnap, &xModule)) {
            if (!strncmp((char*)xModule.szModule, module, 8)) {
                CloseHandle(hSnap);
                return (DWORD)xModule.modBaseSize;
            }
        }
    }
    CloseHandle(hSnap);
    return 0;
}