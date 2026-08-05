#include "sdk/create_interface.h"

#include <Windows.h>

typedef void *(__cdecl *tCreateInterface)(const char *name, int *returnCode);

void *GetInterface(const char *dllName, const char *interfaceName) {
    tCreateInterface CreateInterface =
        (tCreateInterface)GetProcAddress(GetModuleHandleA(dllName), "CreateInterface");

    int returnCode = 0;
    return CreateInterface(interfaceName, &returnCode);
}
