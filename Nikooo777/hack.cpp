//
// Created by Niko on 7/22/2021.
//
#include "common.h"
#include "bhop.h"
#include "norecoil.h"
#include "triggerbot.h"
#include "aimbot.h"
#include "Helper.h"
#include "../minhook/includes/MinHook.h"


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
	std::cout << "clientState offset: engine.dll + 0x" << std::hex << helper->GetClientStateAddress() - helper->GetModule("engine.dll") << std::endl; //this is wrong
	std::cout << "ViewAngles: clientState + 0x4b84" << std::endl;
//
//    std::cout << "localPlayer/EntityList: 0x" << std::hex << helper->GetLocalPlayer() << std::endl; //wtf this is probably wrong
//    std::cout << "ingame players: " << std::dec << (int) *(DWORD **) (helper->GetModule("server.dll") + dwNumPlayers) << std::endl;
	std::cout << "my position:" << helper->GetLocalPlayer()->m_vecPos << std::endl;
}

typedef bool(__thiscall *createMove)(void *thisPtr, float, CUserCmd *);

createMove originalCreateMove = nullptr;

bool __fastcall myCreateMove(void *thisptr, void *not_edx, float flInputSampleTime, CUserCmd *userCmd) {
	bhop();
	triggerBot();
	if (GetAsyncKeyState(VK_LBUTTON) & BUTTON_DOWN) {
		aimbot(userCmd);
	}
	if (GetAsyncKeyState(VK_INSERT) & 1) {
		printInfo();
		Sleep(10); //pressing insert will prevent the rest of the cheats from executing for 10ms...
	}
	noRecoil();
	return originalCreateMove(thisptr, flInputSampleTime, userCmd);
};

DWORD __stdcall mainLoop(void *pParam) {
	//init a console for debug output
	FILE *pFile = nullptr;
	AllocConsole();
	SetConsoleTitle("Nikooo777's H4X!337");
	freopen_s(&pFile, "CONOUT$", "w", stdout);
	std::cout << "injected Nikooo777!" << std::endl;

	auto clientMode = Helper::getInstance()->GetClientMode();

	auto *addrOfCreateMove = (DWORD *) (((*(DWORD **) (*(DWORD ***) clientMode))[21]));

	// Initialize MinHook.
	if (MH_Initialize() != MH_OK) {
		return 1;
	}

	if (MH_CreateHook((LPVOID *) addrOfCreateMove, (LPVOID *) &myCreateMove, reinterpret_cast<LPVOID *>(&originalCreateMove)) != MH_OK) {
		return 1;
	}
	//relative offset
	auto origAddr = (uint32_t) addrOfCreateMove;
	auto gateway = (uint32_t) originalCreateMove;
	uint32_t offset = gateway > origAddr ? gateway - origAddr : origAddr - gateway;
	std::cout << "original: 0x" << std::hex << origAddr << " gateway: 0x" << gateway << " diff: " << offset << std::endl;
	if (MH_EnableHook((LPVOID *) addrOfCreateMove) != MH_OK) {
		return 1;
	}

	printInfo();
	while (!GetAsyncKeyState(VK_END)) {
		Sleep(10);
	}
	std::cout << "Exiting!" << std::endl;
	if (MH_DisableHook((LPVOID *) addrOfCreateMove) != MH_OK) {
		return 1;
	}
	// Uninitialize MinHook.
	if (MH_Uninitialize() != MH_OK) {
		return 1;
	}

	Sleep(2000);

	//release the debug console handles
	fclose(pFile);
	FreeConsole();

	//detach the DLL from the main process
	FreeLibraryAndExitThread(static_cast<HMODULE>(pParam), NULL);
}