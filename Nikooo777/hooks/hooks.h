#pragma once

#include <Windows.h>
#include <d3d9.h>

#include "core/arch.h"

class CUserCmd;
class Vector3;
struct CViewSetup;

namespace hooks {

// Originals (filled by MinHook).
using CreateMoveFn = bool(ARCH_THISCALL *)(void *, float, CUserCmd *);
using OverrideViewFn = void(ARCH_THISCALL *)(void *, CViewSetup *);
using LockCursorFn = void(ARCH_THISCALL *)(void *);
using EndSceneFn = HRESULT(__stdcall *)(IDirect3DDevice9 *);
#if ARCH_X64()
using ClientFireBulletsFn = void (*)(int, const Vector3 *, const Vector3 *,
                                     int, int, int, float, float, float);
using UpdateAccuracyPenaltyFn = void (*)(void *);
#endif

extern CreateMoveFn originalCreateMove;
extern OverrideViewFn originalOverrideView;
extern LockCursorFn originalLockCursor;
extern EndSceneFn originalEndScene;
#if ARCH_X64()
extern ClientFireBulletsFn originalClientFireBullets;
extern UpdateAccuracyPenaltyFn originalUpdateAccuracyPenalty;
#endif

// Hook implementations (MinHook targets). x86 member hooks use __fastcall
// with an EDX placeholder to receive `this` in ECX; MSVC x64 has one calling
// convention, so there the parameter list matches the target exactly.
#if ARCH_X86()
bool __fastcall hkCreateMove(void *thisPtr, void *edx, float flInputSampleTime, CUserCmd *userCmd);
void __fastcall hkOverrideView(void *thisPtr, void *edx,
                               CViewSetup *viewSetup);
void __fastcall hkLockCursor(void *thisPtr, void *edx);
#else
bool hkCreateMove(void *thisPtr, float flInputSampleTime, CUserCmd *userCmd);
void hkOverrideView(void *thisPtr, CViewSetup *viewSetup);
void hkLockCursor(void *thisPtr);
void hkClientFireBullets(int playerIndex, const Vector3 *origin,
                         const Vector3 *fireAngles, int weaponId, int mode,
                         int seed, float inaccuracy, float spread,
                         float soundTime);
void hkUpdateAccuracyPenalty(void *weapon);
#endif
HRESULT __stdcall hkEndScene(IDirect3DDevice9 *device);
void ShutdownEndScene();

// Lifecycle: install hooks, run until END, then tear down.
// pModule is the DLL HMODULE for FreeLibraryAndExitThread.
DWORD __stdcall MainThread(void *pModule);

} // namespace hooks
