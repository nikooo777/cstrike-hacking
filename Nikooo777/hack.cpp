//
// Created by Niko on 7/22/2021.
//
#include "common.h"
#include "bhop.h"
#include "norecoil.h"
#include "triggerbot.h"
#include "aimbot.h"
#include "Helper.h"
#include "sdk/sdk.h"
#include "BaseClient.h"
#include "../minhook/includes/MinHook.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_dx9.h"
#include "../imgui/imgui_impl_win32.h"
#include <d3d9.h>
#include <d3dx9.h>


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

typedef void (__thiscall *FrameStageNotify)(void *thisPtr, ClientFrameStage_t curStage);

typedef HRESULT (__stdcall *EndScene)(LPDIRECT3DDEVICE9 pDevice);

createMove originalCreateMove = nullptr;
FrameStageNotify originalFrameStageNotify = nullptr;
EndScene originalEndScene = nullptr;
IDirect3D9 *pD3D = nullptr;
HWND window = nullptr;
IDirect3DDevice9 *pDevice = nullptr;

bool __fastcall myCreateMove(void *thisPtr, void *not_edx, float flInputSampleTime, CUserCmd *userCmd) {
	bhop();
	triggerBot();
	if (GetAsyncKeyState(VK_LBUTTON) & BUTTON_DOWN) {
		aimbot(userCmd);
	}
	if (GetAsyncKeyState(VK_INSERT) & 1) {
		printInfo();
		Sleep(10); //pressing insert will prevent the rest of the cheats from executing for 10ms...
	}
	noRecoil(userCmd);
	return originalCreateMove(thisPtr, flInputSampleTime, userCmd);
};

void __fastcall myFrameStageNotify(void *thisPtr, void *edx, ClientFrameStage_t curStage) {
	//used for visual no recoil
	static Vector3 oldPunch, *pPunch = nullptr;
	if (curStage == FRAME_RENDER_START) {
		auto *localPlayer = Helper::getInstance()->GetLocalPlayer();
		if (localPlayer) {
			pPunch = &localPlayer->m_Local.m_vecPunchAngle;
			if (pPunch && pPunch->x != 0 && pPunch->y != 0 && pPunch->z != 0) {
				oldPunch = *pPunch;
				pPunch->Zero();
			}
		} else {
			pPunch = nullptr;
		}
	}
	originalFrameStageNotify(thisPtr, curStage);
	if (pPunch) {
		*pPunch = oldPunch;
	}
}

HRESULT __stdcall myEndScene(LPDIRECT3DDEVICE9 pDevice) {
	static bool init = false;
	if (!init) {
		MessageBoxA(NULL, "EndScene", "EndScene", MB_OK);
		init = true;
	}
	return originalEndScene(pDevice);
}

BOOL CALLBACK EnumWindowsCallback(HWND handle, LPARAM lParam) {
	DWORD wndProcId;
	GetWindowThreadProcessId(handle, &wndProcId);
	DWORD processId = GetCurrentProcessId();

	if (processId != wndProcId)
		return TRUE;

	window = handle;
	return FALSE;
}

HWND GetProcessWindow() {
	EnumWindows(EnumWindowsCallback, NULL);
	return window;
}

bool GetD3D9Device(void **pTable, size_t size) {
	if (!pTable) {
		return false;
	}

	pD3D = Direct3DCreate9(D3D_SDK_VERSION);
	if (!pD3D) {
		return false;
	}

	D3DPRESENT_PARAMETERS d3dpp = {};
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.hDeviceWindow = GetProcessWindow();
	d3dpp.Windowed = true;

	HRESULT res = pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3dpp.hDeviceWindow, D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dpp, &pDevice);

	if (FAILED(res)) {
		return false;
	}
	if (!pDevice) {
		pDevice->Release();
		return false;
	}
	memcpy(pTable, *reinterpret_cast<void ***>(pDevice), size);

	pD3D->Release();
	pDevice->Release();
	return true;
}

void CleanupDeviceD3D() {
	if (pDevice) {
		pDevice->Release();
		pDevice = nullptr;
	}
	if (pD3D) {
		pD3D->Release();
		pD3D = nullptr;
	}
}


DWORD __stdcall mainLoop(void *pParam) {
	//init a console for debug output
	FILE *pFile = nullptr;
	AllocConsole();
	SetConsoleTitle("Nikooo777's H4X!337");
	freopen_s(&pFile, "CONOUT$", "w", stdout);
	std::cout << "injected Nikooo777!" << std::endl;

	//get address of IBaseClient with the use of CreateInterface
	auto *baseClient = (BaseClient *) GetInterface("client.dll", "VClient017");
	if (baseClient == nullptr) {
		std::cout << "BaseClient is null!" << std::endl;
		return 1;
	}
	//print the address of IBaseClient
	std::cout << "BaseClient: 0x" << std::hex << baseClient << std::endl;
	// Get a pointer to the vtable
	void ***vtable = (void ***) baseClient;

	// Get the address of FrameStageNotify from the vtable
	//proof: https://scrn.storni.info/2023-08-01_02-32-55.png
	void *frameStageNotifyAddress = (*vtable)[35];

	// Print the address
	std::cout << "FrameStageNotify: 0x" << std::hex << frameStageNotifyAddress << std::endl;

	auto clientMode = Helper::getInstance()->GetClientMode();

	auto *addrOfCreateMove = (DWORD *) (((*(DWORD **) (*(DWORD ***) clientMode))[21]));


	// Initialize MinHook.
	if (MH_Initialize() != MH_OK) {
		return 1;
	}
	void *d3d9Device[119];
	if (GetD3D9Device(d3d9Device, sizeof(d3d9Device))) {
		std::cout << "d3d9Device: 0x" << std::hex << d3d9Device << std::endl;
	} else {
		std::cout << "d3d9Device is null!" << std::endl;
	}

	//hook createMove
	if (MH_CreateHook((LPVOID *) addrOfCreateMove, (LPVOID *) &myCreateMove, reinterpret_cast<LPVOID *>(&originalCreateMove)) != MH_OK) {
		return 1;
	}
	//hook frameStageNotify
	if (MH_CreateHook(frameStageNotifyAddress, (LPVOID *) &myFrameStageNotify, reinterpret_cast<LPVOID *>(&originalFrameStageNotify)) != MH_OK) {
		return 1;
	}

	//hook endScene
	if (MH_CreateHook((LPVOID *) d3d9Device[42], (LPVOID *) &myEndScene, reinterpret_cast<LPVOID *>(&originalEndScene)) != MH_OK) {
		CleanupDeviceD3D();
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
	if (MH_EnableHook(frameStageNotifyAddress) != MH_OK) {
		return 1;
	}
	if (MH_EnableHook((LPVOID *) d3d9Device[42]) != MH_OK) {
		CleanupDeviceD3D();
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
	if (MH_DisableHook(frameStageNotifyAddress) != MH_OK) {
		return 1;
	}
	if (MH_DisableHook((LPVOID *) d3d9Device[42]) != MH_OK) {
		CleanupDeviceD3D();
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