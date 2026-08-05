#pragma once

#include <Windows.h>
#include <d3d9.h>

#include "sdk/base_client.h"
#include "sdk/user_cmd.h"

namespace hooks {

// Originals (filled by MinHook).
using CreateMoveFn = bool(__thiscall *)(void *, float, CUserCmd *);
using FrameStageNotifyFn = void(__thiscall *)(void *, ClientFrameStage_t);
using EndSceneFn = HRESULT(__stdcall *)(IDirect3DDevice9 *);

extern CreateMoveFn originalCreateMove;
extern FrameStageNotifyFn originalFrameStageNotify;
extern EndSceneFn originalEndScene;

// Hook implementations (MinHook targets).
bool __fastcall hkCreateMove(void *thisPtr, void *edx, float flInputSampleTime, CUserCmd *userCmd);
void __fastcall hkFrameStageNotify(void *thisPtr, void *edx, ClientFrameStage_t curStage);
HRESULT __stdcall hkEndScene(IDirect3DDevice9 *device);
void ShutdownEndScene();

// Lifecycle: install hooks, run until END, then tear down.
// pModule is the DLL HMODULE for FreeLibraryAndExitThread.
DWORD __stdcall MainThread(void *pModule);

} // namespace hooks
