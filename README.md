# Source engine reverse-engineering tutorial

Learn how Counter-Strike: Source fits together by building a small **32-bit internal experiment DLL**. This is a personal reverse-engineering and systems exercise, not a ready-to-use product.

> **Safety note:** Use this only for source study and permitted, offline/local testing. There is no VAC bypass, anti-cheat defeat, injector, or online-testing workflow here. Do not load it into protected multiplayer or someone else's process.

## Why this exists

I enjoy Source games and programming. C++ is not my favorite language, but it is the right tool for exploring client state, input, entities, and rendering at this level.

The practical goal is defensive understanding: learn what a client-side tool can read and change so I can better protect a large Counter-Strike: Source community. The code is intentionally small enough to follow while learning.

## Why CS:S?

- It is still played and interesting to study.
- It is old enough that updates are relatively infrequent, so signatures and offsets remain useful for longer.
- Its architecture is close enough to other Source-era games for research notes to transfer.
- There are enough public write-ups and dumps to cross-check findings.

The first guide I used was [Guided Hacking's beginner guide](https://guidedhacking.com/threads/ghb1-start-here-beginner-guide-to-game-hacking.5911/).

## What works today

This is a teaching codebase tied to one Counter-Strike: Source client build. It currently builds a DLL with CMake and MSVC x86 and contains these experiments:

| Experiment | Hook / input | Implementation |
|------------|--------------|----------------|
| Bunny hop | `CreateMove` / hold **Space** | `features/bhop.cpp` writes the client's force-jump command. |
| Aimbot | `CreateMove` / hold **LMB** | `features/aimbot.cpp` chooses the closest valid enemy and aims at bone 14. |
| Triggerbot | `CreateMove` / hold **Shift** | `features/triggerbot.cpp` uses the crosshair entity ID and fires only for a valid, grounded target. |
| Command no-recoil | `CreateMove` | `features/norecoil.cpp` compensates view angles using the local punch angle. |
| Visual no-recoil | `FrameStageNotify` | Temporarily hides the punch angle during `FRAME_RENDER_START`, then restores it. |
| ImGui menu | `EndScene` + window procedure | `features/menu.cpp` exposes runtime toggles and is opened with **Insert**. |
| Debug dump | `CreateMove` / **F1** | `features/debug_info.cpp` prints module bases, offsets, client state, and local position. |

All gameplay experiments start enabled by default; the menu itself starts closed. MinHook installs `CreateMove`, `FrameStageNotify`, and D3D9 `EndScene`. The window procedure is replaced separately so **Insert** works even when `CreateMove` is not running.

### In-game controls

| Key | Action |
|-----|--------|
| **INSERT** | Toggle the ImGui menu. |
| **F1** | Print the debug dump to the allocated console. |
| **END** | Disable hooks, restore the window procedure, and unload the DLL. |
| **Space** | Hold for bunny hop when enabled. |
| **Shift** | Hold for triggerbot when enabled. |
| **LMB** | Hold for the aimbot when enabled. |

The menu is initialized on the first successful D3D9 `EndScene` call. If it is closed, the hook skips ImGui's `NewFrame` and render work; **Insert** is still handled by the window procedure.

## Runtime flow

The DLL keeps the entry point small and does the work on a worker thread:

~~~text
DllMain (process attach)
  -> hooks::MainThread
       -> load signatures.ini beside the DLL and apply runtime settings
       -> resolve VClient017, ClientMode, ClientState, and a D3D9 vtable
       -> install MinHook hooks
       -> run feature code from the appropriate callback

CreateMove          -> bhop, triggerbot, aimbot, command no-recoil, F1 debug
FrameStageNotify    -> temporary visual no-recoil adjustment
EndScene            -> ImGui frame/render when the menu is open
Window procedure     -> Insert toggle and ImGui input
END                 -> disable hooks, shut down ImGui, unload the DLL
~~~

## Project layout

```text
Nikooo777/
  dllmain.cpp              # DllMain only — starts the main thread
  core/                    # constants, netvar-ish offsets, padding macros, module bases
  config/                  # runtime INI loader
  memory/                  # pattern scanner (ScanModCombo, module size, …)
  math/                    # Vector3 (POD so it works in overlay unions)
  sdk/                     # Source-like types only (no feature logic)
    entity/                # CLocal, CBasePlayer, CCSPlayer
    user_cmd.h, client_*.h, create_interface.*
  game/                    # live game access
    entity_list.*          # local player + entity list
    interfaces.*           # ClientMode / ClientState / BaseClient resolve
    player.*               # IsAlive, IsEnemy, IsValidTarget, EyePosition
  hooks/                   # MinHook lifecycle + individual hooks + dummy D3D device
  features/                # gameplay logic + menu + config flags
imgui/                     # Dear ImGui + DX9 / Win32 backends
minhook/                   # headers + prebuilt x86 libs (v141)
config/                    # signatures.ini patterns, settings, and provenance
```

### Layer rules (keep the tutorial readable)

1. **`sdk/`** — memory layouts and interface stubs. No hooks, no features.
2. **`game/`** — how we *find* and *read* live objects (signatures, entity list).
3. **`features/`** — what we *do* with that data. Prefer `game::` helpers over copy-pasted field checks.
4. **`hooks/`** — only place that installs MinHook / D3D and calls into features.
5. **`dllmain.cpp`** — attach / detach only.

Entity members use absolute `this + offset` accessors (`DEFINE_MEMBER`) so `CCSPlayer : public CBasePlayer` stays layout-safe. Standalone overlays (e.g. `CLocal`) still use pad unions (`DEFINE_MEMBER_N`).

### Where to add something new

| Goal | Place |
|------|--------|
| New cheat feature | `features/foo.*` → call from `hooks/create_move.cpp` (logic) or `hooks/end_scene.cpp` (draw) → add `.cpp` to `CMakeLists.txt` → optional toggle in `features/config.h` + menu |
| New player / entity field | `sdk/entity/` |
| Global address (force jump, entity list, …) | `core/offsets.h` |
| New interface / signature | Add the pattern, operand rule, and provenance to `config/signatures.ini`; keep resolution logic in `game/interfaces.cpp` and explain discovery in `aidocs/`. |
| Shared target / eye helpers | `game/player.*` |

## Building

### Requirements

- **Windows**, target **Win32 (x86)** — CS:S is a 32-bit process; a 64-bit DLL will not load correctly
- **CMake** ≥ 3.19
- **MSVC** with a Win32 toolset (VS 2019 Build Tools work; MinHook libs are `v141` x86)
- **[DirectX SDK (June 2010)](https://www.microsoft.com/en-us/download/details.aspx?id=6812)** — `d3d9` only (ImGui’s DX9 backend does not need D3DX). Default path in CMake; override with `-DDXSDK_DIR=...`
- **C++17**

Dear ImGui and the MinHook headers/libraries are included in the repository, so no package manager is required. The CMake file links only `d3d9`; ImGui’s DX9 backend does not require D3DX.

The MinHook libraries in `minhook/lib/` are prebuilt x86 `v141` libraries. VS 2019 Build Tools are the baseline used here; a compatible newer MSVC toolset should also use an x86/Win32 profile.

For CLion, select an **MSVC x86** CMake profile. Its bundled MinGW toolchain is usually x86_64-only and will not produce a proper CS:S DLL. With Visual Studio, choose the Win32 platform.

### MSVC x86 command line

Open an **x86 Native Tools Command Prompt for Visual Studio**. If you start from a regular cmd.exe, initialize the x86 environment first:

```bat
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86
```

Configure and build a Debug DLL:

```bat
cmake -S . -B build-msvc-x86 -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug
cmake --build build-msvc-x86 --target nikooo777
```

If the DirectX SDK is installed elsewhere, point DXSDK_DIR at its root directory—the directory containing Include and Lib:

```bat
cmake -S . -B build-msvc-x86 -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug -DDXSDK_DIR="D:/SDKs/DirectX SDK (June 2010)"
```

Debug output: `build-msvc-x86/nikooo777.dll` (links `libMinHook-x86-v141-mdd.lib`).

The build also copies `config/signatures.ini` to `build-msvc-x86/signatures.ini`, beside the DLL. Edit the checked-in file, then rebuild before testing a new binary.


For a single-config NMake Release build:

```bat
cmake -S . -B build-msvc-x86-release -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build-msvc-x86-release --target nikooo777
```

The single-config Release command selects the non-debug MinHook library. For a multi-configuration Visual Studio generator, use `cmake --build <build-dir> --config Debug` or `--config Release`; the current CMake file chooses MinHook from `CMAKE_BUILD_TYPE`, so verify that link before building Release. Use a fresh build directory when switching architectures.

## Permitted offline smoke test

This repository does not provide a standalone executable, injector, or anti-cheat workaround. For a local test environment you control:

1. Build the DLL for Win32/x86.
2. Start a permitted offline or local Counter-Strike: Source session.
3. Load the DLL using an injector you already trust and are authorized to use.
4. Check the console for `BaseClient`, `ClientMode`, `FrameStageNotify`, `CreateMove`, and `EndScene` addresses.
5. Press **F1** to print the module/offset dump, then **Insert** to open the menu.
6. Press **End** to restore hooks and unload cleanly.
7. Confirm the console shows the loaded config path and the signature provenance/match offsets before treating a resolution as valid.

The addresses, offsets, and signatures are build-specific. A successful DLL build does not mean that it is safe to load into a different game binary.

## Offsets, signatures, and vtables

These are the files to revisit when the client build changes:

| File | What it contains |
|------|------------------|
| `core/offsets.h` | Global client/server addresses such as the entity list and force commands. |
| `sdk/entity/*.h` | Absolute entity-field offsets and the padded `m_Local` layout. |
| `config/signatures.ini` | Runtime patterns, module names, operand offsets, pointer indirections, validation settings, feature defaults, and discovery links. |
| `game/interfaces.cpp` | `CreateInterface` lookup plus the configured ClientState and ClientMode resolution logic. |
| `hooks/hooks.cpp` | Vtable slots for `CreateMove`, `FrameStageNotify`, and `EndScene`. |
| `core/constants.h` | Entity stride, player limits, team values, and movement flags. |

The debug dump reports module bases, the configured offsets, the resolved ClientState address, and the local player position when one is available. If a signature is not found, or a read produces null/garbage data, treat the binary and the offsets as mismatched and re-dump them rather than guessing.

## Tutorial notes

- [001 - Signature scanning, offsets, and pointer derivation](aidocs/001_signature-scanning-and-offsets.md)

`config/signatures.ini` is the source of truth for the two current runtime signatures. It intentionally records how each pattern was found, not just the bytes: update the provenance fields whenever a new build is reverse-engineered.

## Troubleshooting

| Symptom | Likely cause / next check |
|---------|---------------------------|
| CMake warns about a 64-bit toolchain | Select an MSVC **x86/Win32** toolchain and configure a new build directory. |
| `d3d9.h` or `d3d9.lib` is missing | Set `DXSDK_DIR` to the DirectX SDK root and verify Include/d3d9.h plus Lib/x86/d3d9.lib exist. |
| A MinHook library cannot be opened | Use the matching x86 library in minhook/lib/; Debug selects mdd, Release selects md for the single-config command above. |
| `signatures.ini` cannot be loaded | Build from the repository so CMake copies `config/signatures.ini` beside the DLL; do not launch with a stale or missing adjacent config. |
| `ClientState signature not found` or `ClientMode signature not found` | The byte pattern is for another client build. Confirm the executable/module version and update the pattern. |
| `BaseClient is null` | `VClient017` was not exposed by the loaded client module, or the DLL was loaded at the wrong time/process. |
| D3D9 capture fails or the menu never appears | The code needs a visible, suitably sized game window and a D3D9 device. Wait until the game window is initialized and verify that the target is using D3D9. |
| A feature crashes or reads implausible values | Stop testing: an entity/global offset is stale or the target is not the expected 32-bit build. |

## Known limitations

- Offsets, signatures, and vtable assumptions are tied to the client build this project was developed against.
- Memory access is direct and lightly validated; this is experimental code, not a hardened runtime.
- The aimbot is intentionally basic: closest valid target, bone 14, and an immediate angle change. It does not implement visibility checks, smoothing, weapon handling, or movement correction.
- The triggerbot is deliberately narrow: it uses the crosshair ID and requires the local player to be on the ground.
- Rendering support is D3D9-specific and depends on finding the game's visible top-level window.
- There is no automated test suite; validation is a successful x86 build followed by a permitted local smoke test.
- Nothing here is intended to bypass VAC, FaceIT, or any other anti-cheat system.

## Videos / notes while reversing

| Topic | Link |
|-------|------|
| Finding ClientState | [Odysee](https://odysee.com/@Swiss-Experiments:a/finding-clientstate-with-ida-on-counter:7) · [YouTube](https://www.youtube.com/watch?v=J6vO-ANi4Q8) |
| Finding view angles | [Odysee](https://odysee.com/@Swiss-Experiments:a/finding-viewangles-with-ida-for-counter:e) · [YouTube](https://www.youtube.com/watch?v=mS8ZQ5N7Dvk) |
| Finding bone matrix | [Odysee](https://odysee.com/@Swiss-Experiments:a/how-to-locate-bonematrix:5) · [YouTube](https://www.youtube.com/watch?v=elKUMiqitxY) |

Offsets in `core/offsets.h` and the entity headers are for the client build this project was developed against. If your CS:S binary differs, re-dump.

The signature records point back to this section and the numbered tutorial so the pattern bytes, operand offsets, and pointer-chain assumptions can be re-derived instead of copied blindly.

## Status / honesty

- Code quality is tutorial and experimental, not production.
- There is no support for protected multiplayer or anti-cheat bypass.
- If you need a maintained end-user package, this repository is intentionally not that.

## End goal

Understand the client well enough to defend a large CS:S community: know what cheats can see and do, and how to reason about them when they show up on the server.
