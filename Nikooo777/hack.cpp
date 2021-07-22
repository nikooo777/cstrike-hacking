//
// Created by Niko on 7/22/2021.
//
#include "common.h"

#include "bhop.h"
#include "norecoil.h"
#include "triggerbot.h"

void printInfo() {
    auto helper = Helper::getInstance();
    std::cout << "clientModuleBase: 0x" << std::hex << helper->GetModule("client.dll") << std::endl;
    std::cout << "serverModuleBase: 0x" << std::hex << helper->GetModule("server.dll") << std::endl;
    std::cout << "engineModuleBase: 0x" << std::hex << helper->GetModule("engine.dll") << std::endl;

    std::cout << "entityListOffset: 0x" << std::hex << dwEntityList << std::endl;
    std::cout << "forceJumpOffset: 0x" << std::hex << dwForceJump << std::endl;
    std::cout << "numPlayersOffset: 0x" << std::hex << dwNumPlayers << std::endl;
    std::cout << "forceAttack1Offset: 0x" << std::hex << dwForceAttack1 << std::endl;
    std::cout << "forceAttack2Offset: 0x" << std::hex << dwForceAttack2 << std::endl;
    std::cout << "clientState addr: 0x" << std::hex << helper->GetClientStateAddress() << std::endl;
    std::cout << "clientState offset: engine.dll + 0x" << std::hex << helper->GetClientStateAddress() - helper->GetModule("engine.dll") << std::endl; //this is wrong
    std::cout << "ViewAngles: clientState + 0x4b84" << std::endl;

    std::cout << "localPlayer/EntityList: 0x" << std::hex << helper->GetLocalPlayer() << std::endl; //wtf this is probably wrong
    std::cout << "ingame players: " << std::dec << (int) *(DWORD **) (helper->GetModule("server.dll") + dwNumPlayers) << std::endl;
}

DWORD __stdcall mainLoop(void *pParam) {
    //init a console for debug output
    FILE *pFile = nullptr;
    AllocConsole();
    SetConsoleTitle("Nikooo777's H4X!337");
    freopen_s(&pFile, "CONOUT$", "w", stdout);
    std::cout << "injected Nikooo777!" << std::endl;


    printInfo();
    while (!GetAsyncKeyState(VK_END)) {
        bhop();
        triggerBot();
        noRecoil();
        if (GetAsyncKeyState(VK_INSERT) & 1) {
            printInfo();
            Sleep(10); //pressing insert will prevent the rest of the cheats from executing for 10ms...
        }
    }
    std::cout << "Exiting!" << std::endl;
    Sleep(2000);

    //release the debug console handles
    fclose(pFile);
    FreeConsole();

    //detach the DLL from the main process
    FreeLibraryAndExitThread(static_cast<HMODULE>(pParam), NULL);
}