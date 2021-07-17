//
// Created by Niko on 7/12/2021.
//

#ifndef CSTRIKE_HAX_MEM_H
#define CSTRIKE_HAX_MEM_H


char* ScanBasic(char* pattern, char* mask, char* begin, intptr_t size);
char* ScanInternal(char* pattern, char* mask, char* begin, intptr_t size);
void Parse(char *combo, char *pattern, char *mask);
char* ScanModCombo(char* comboPattern, char *begin, intptr_t size);
DWORD GetModuleSize(DWORD processID, char* module);
#endif //CSTRIKE_HAX_MEM_H
