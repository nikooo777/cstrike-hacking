//
// Created by Niko on 8/1/2023.
//
#include "sdk.h"

typedef void *(__cdecl *tCreateInterface)(const char *name, int *returnCode);

void *GetInterface(const char *dllName, const char *interfaceName) {
	tCreateInterface CreateInterface = (tCreateInterface) GetProcAddress(GetModuleHandle(dllName), "CreateInterface");

	int returnCode = 0;
	void *iface = CreateInterface(interfaceName, &returnCode);

	return iface;
}