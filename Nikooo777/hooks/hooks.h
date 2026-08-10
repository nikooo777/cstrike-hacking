#pragma once

#include <Windows.h>
#include <d3d9.h>

class CUserCmd;
class Vector3;
struct CViewSetup;

namespace hooks {

// Originals (filled by MinHook).
#if defined(_M_IX86) || defined(__i386__)
using CreateMoveFn = bool(__thiscall *)(void *, float, CUserCmd *);
using OverrideViewFn = void(__thiscall *)(void *, CViewSetup *);
using LockCursorFn = void(__thiscall *)(void *);
using EndSceneFn = HRESULT(__stdcall *)(IDirect3DDevice9 *);
#else
// MSVC x64 has one calling convention and does not pass the x86 __fastcall
// EDX placeholder. Keep the hook's parameter list identical to the target.
using CreateMoveFn = bool (*)(void *, float, CUserCmd *);
using OverrideViewFn = void (*)(void *, CViewSetup *);
using LockCursorFn = void (*)(void *);
using EndSceneFn = HRESULT (*)(IDirect3DDevice9 *);
using ClientFireBulletsFn = void (*)(int, const Vector3 *, const Vector3 *,
                                     int, int, int, float, float, float);
using UpdateAccuracyPenaltyFn = void (*)(void *);
#endif

extern CreateMoveFn originalCreateMove;
extern OverrideViewFn originalOverrideView;
extern LockCursorFn originalLockCursor;
extern EndSceneFn originalEndScene;
#if !defined(_M_IX86) && !defined(__i386__)
extern ClientFireBulletsFn originalClientFireBullets;
extern UpdateAccuracyPenaltyFn originalUpdateAccuracyPenalty;
#endif

// Hook implementations (MinHook targets).
#if defined(_M_IX86) || defined(__i386__)
bool __fastcall hkCreateMove(void *thisPtr, void *edx, float flInputSampleTime, CUserCmd *userCmd);
void __fastcall hkOverrideView(void *thisPtr, void *edx,
                               CViewSetup *viewSetup);
void __fastcall hkLockCursor(void *thisPtr, void *edx);
HRESULT __stdcall hkEndScene(IDirect3DDevice9 *device);
#else
bool hkCreateMove(void *thisPtr, float flInputSampleTime, CUserCmd *userCmd);
void hkOverrideView(void *thisPtr, CViewSetup *viewSetup);
void hkLockCursor(void *thisPtr);
HRESULT hkEndScene(IDirect3DDevice9 *device);
void hkClientFireBullets(int playerIndex, const Vector3 *origin,
                         const Vector3 *fireAngles, int weaponId, int mode,
                         int seed, float inaccuracy, float spread,
                         float soundTime);
void hkUpdateAccuracyPenalty(void *weapon);
#endif
void ShutdownEndScene();

// Lifecycle: install hooks, run until END, then tear down.
// pModule is the DLL HMODULE for FreeLibraryAndExitThread.
DWORD __stdcall MainThread(void *pModule);

} // namespace hooks
