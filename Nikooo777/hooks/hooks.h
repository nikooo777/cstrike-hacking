#pragma once

#include <Windows.h>
#include <d3d9.h>

#include "sdk/base_client.h"
#include "sdk/user_cmd.h"

namespace hooks {

// Originals (filled by MinHook).
#if defined(_M_IX86) || defined(__i386__)
using CreateMoveFn = bool(__thiscall *)(void *, float, CUserCmd *);
using FrameStageNotifyFn = void(__thiscall *)(void *, ClientFrameStage_t);
using EndSceneFn = HRESULT(__stdcall *)(IDirect3DDevice9 *);
#else
// MSVC x64 has one calling convention and does not pass the x86 __fastcall
// EDX placeholder. Keep the hook's parameter list identical to the target.
using CreateMoveFn = bool (*)(void *, float, CUserCmd *);
using FrameStageNotifyFn = void (*)(void *, ClientFrameStage_t);
using EndSceneFn = HRESULT (*)(IDirect3DDevice9 *);
#endif

extern CreateMoveFn originalCreateMove;
extern FrameStageNotifyFn originalFrameStageNotify;
extern EndSceneFn originalEndScene;

// Hook implementations (MinHook targets).
#if defined(_M_IX86) || defined(__i386__)
bool __fastcall hkCreateMove(void *thisPtr, void *edx, float flInputSampleTime, CUserCmd *userCmd);
void __fastcall hkFrameStageNotify(void *thisPtr, void *edx, ClientFrameStage_t curStage);
HRESULT __stdcall hkEndScene(IDirect3DDevice9 *device);
#else
bool hkCreateMove(void *thisPtr, float flInputSampleTime, CUserCmd *userCmd);
void hkFrameStageNotify(void *thisPtr, ClientFrameStage_t curStage);
HRESULT hkEndScene(IDirect3DDevice9 *device);
#endif
void ShutdownEndScene();

// Lifecycle: install hooks, run until END, then tear down.
// pModule is the DLL HMODULE for FreeLibraryAndExitThread.
DWORD __stdcall MainThread(void *pModule);

} // namespace hooks
