#include <iostream>
#include <Windows.h>
#include <thread>

#define SPACE_DOWN 0x8000

struct gameOffsets {
    DWORD dwEntityList = 0x4D5AE4;
    DWORD dwForceJump = 0x4F5D24;
    DWORD fFlags = 0x350;
} offsets;

struct values {
    DWORD localPlayer;
    DWORD moduleBase;
    BYTE flag;
} val;

void console() {
    FILE *pFile = nullptr;
    AllocConsole();
    SetConsoleTitle("Nikooo777's H4X!337");
    freopen_s(&pFile, "CONOUT$", "w", stdout);
    std::cout << "injected Nikooo777!" << std::endl;
    while (val.localPlayer == NULL) {
        Sleep(10);
    }
    std::cout << "moduleBase: 0x" << std::hex << val.moduleBase<< std::endl;
    std::cout << "localPlayer: 0x" << std::hex << val.localPlayer<<" (0x"<< val.moduleBase<<"+0x"<<offsets.dwEntityList <<")"<< std::endl;

    while (!GetAsyncKeyState(VK_END)) {
        if (GetAsyncKeyState(VK_INSERT) & 1) {
            std::cout << "Test!" << std::endl;
        }
        Sleep(1);
    }
    std::cout << "Exiting!" << std::endl;
    Sleep(2000);
    fclose(pFile);
    FreeConsole();
}

DWORD __stdcall bHop(void *pParam) {
    std::thread t1(console);
    val.moduleBase = reinterpret_cast<DWORD>(GetModuleHandleA("client.dll"));
    while (val.localPlayer == NULL) {
        val.localPlayer = *(DWORD *) (val.moduleBase + offsets.dwEntityList);
    }

    while (!GetAsyncKeyState(VK_END)) {
        if (GetAsyncKeyState(VK_SPACE) & SPACE_DOWN) {
            val.flag = *(BYTE *) (val.localPlayer + offsets.fFlags);
            uintptr_t buffer = 4;
            if (val.flag & 1) {
                buffer = 5;
            }
            *(DWORD *) (val.moduleBase + offsets.dwForceJump) = buffer;
        }
    }
    t1.join();
    FreeLibraryAndExitThread(static_cast<HMODULE>(pParam), NULL);
}

BOOL WINAPI DllMain(
        HINSTANCE hModule,
        DWORD fdwReason,
        LPVOID lpReserved)
{
    // Perform actions based on the reason for calling.
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            CreateThread(nullptr, 0, bHop, hModule, 0, nullptr);
            break;

        case DLL_PROCESS_DETACH:
            // Perform any necessary cleanup.
            break;
    }
    return TRUE;  // Successful DLL_PROCESS_ATTACH.
}
