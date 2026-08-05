#include "sdk/create_interface.h"

#include <Windows.h>

typedef void *(__cdecl *tCreateInterface)(const char *name, int *returnCode);

void *GetInterface(const char *dllName, const char *interfaceName) {
    const auto module = GetModuleHandleA(dllName);
    if (module == nullptr) {
        return nullptr;
    }

    const auto CreateInterface = reinterpret_cast<tCreateInterface>(
        GetProcAddress(module, "CreateInterface"));
    if (CreateInterface == nullptr) {
        return nullptr;
    }

    int returnCode = 0;
    return CreateInterface(interfaceName, &returnCode);
}
