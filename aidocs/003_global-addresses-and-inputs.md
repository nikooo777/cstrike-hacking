# Global addresses, interfaces, and input commands

The first signature lesson used hardcoded module-relative globals as examples:
the entity list and the force-jump/force-attack slots. That is useful for
learning how a module base turns an offset into a process address, but it is
not automatically the best implementation choice.

Before adding another pattern, ask whether the program already exposes a
named interface or whether the value belongs in the command object passed to a
hook. A semantic API usually gives us better validation and less pointer
arithmetic than a copied global offset.

Use this only with binaries and processes you are permitted to study,
preferably in an offline or local test environment.

## The old global-offset problem

The previous implementation had values like these:

~~~cpp
auto entityListSlot = core::GetModule("client.dll") + dwEntityList;
auto forceJump = core::GetModule("client.dll") + dwForceJump;
~~~

Those constants were module-relative, not absolute process addresses. They were
also tied to one particular build and had no startup validation. A changed
client could make the code read the wrong entity pointer or write a command to
an unrelated location.

The current implementation removes those consumers instead of adding guessed
patterns:

| Old source | Current source of truth |
|---|---|
| `dwEntityList` | `VClientEntityList003::GetClientEntity` |
| `dwForceJump` | `CUserCmd::buttons` and `IN_JUMP` |
| `dwForceAttack1` | `CUserCmd::buttons` and `IN_ATTACK` |
| `dwForceAttack2` | Removed; no feature uses it. |
| `dwNumPlayers`, `dwMaxPlayers` | Removed; they had no consumers. |

This is still an offset lesson: the important work is choosing the correct
source of truth before decoding an address.

## Ghidra evidence for the entity-list interface

In the current Ghidra `client.dll` sample:

1. Search strings for `VClientEntityList003`.
2. The string is at `0x103977EC`.
3. Its data reference is used by the interface-registration code at
   `0x1003E3D0`.
4. That registration record points at a factory stub at `0x100F0530`.
5. The factory stub returns the runtime object stored behind an absolute
   instruction operand. That operand is sample-specific and must not be copied
   as a permanent address.

The durable fact is the interface name, not any of those addresses. The
implementation stores the name and its provenance in `config/signatures.ini`:

~~~ini
[interface.ClientEntityList]
module=client.dll
name=VClientEntityList003
required=true
discovery=In Ghidra, search client.dll strings for VClientEntityList003, follow its interface-registration reference, and verify the IClientEntityList method order before calling GetClientEntity.
source=aidocs/003_global-addresses-and-inputs.md
~~~

The method order is part of the ABI contract. The public Valve
[`IClientEntityList` definition](https://raw.githubusercontent.com/ValveSoftware/source-sdk-2013/master/src/public/icliententitylist.h)
places `GetClientEntity(int)` at virtual slot 3. The local
`sdk/client_entity_list.h` keeps the preceding methods in the declaration so
the compiler makes the same call.

The helper translates the tutorial's player slot to a Source entity index:

~~~cpp
constexpr int kFirstPlayerEntityIndex = 1;
return reinterpret_cast<CCSPlayer *>(
    entityList->GetClientEntity(kFirstPlayerEntityIndex + index));
~~~

Entity index 0 is the world entity in this client family, so player slot 0 is
queried as entity index 1. Verify that convention when moving to another game
or branch.

## Use `CUserCmd` for input buttons

The `CreateMove` hook already receives a `CUserCmd *`. Writing force-jump or
force-attack globals is unnecessary when the desired operation is simply to
set input buttons for the current command.

The hook now calls the original first, then applies feature edits:

~~~cpp
const bool result = originalCreateMove(thisPtr, flInputSampleTime, userCmd);
features::Bhop(userCmd);
features::Triggerbot(userCmd);
features::Aimbot(userCmd);
features::NoRecoil(userCmd);
return result;
~~~

That ordering matters: the original function gets the command populated before
the feature code changes it. Bhop sets or clears `IN_JUMP`; triggerbot sets or
clears `IN_ATTACK` only while its activation key is held. The feature code no
longer writes arbitrary module memory.

## When a signature is still the right answer

This cleanup does not make signatures obsolete. Use the existing
`config/signatures.ini` and the [001 signature-scanning guide](001_signature-scanning-and-offsets.md)
when:

- no named interface exposes the object;
- no hook receives the value in a command or callback argument;
- a global is genuinely required and its instruction site can be identified;
- the operand semantics can be decoded and validated at startup.

For a true global signature, record the module, complete instruction pattern,
operand offset, indirection count, validation rule, and Ghidra discovery path.
Do not turn a one-build observation into a new constant just because the
scanner can find the surrounding bytes.

## Validation checklist

When replacing a global address, check all of these before treating the change
as complete:

1. Confirm the interface or command object exists in the target lifecycle.
2. Confirm the ABI: method order, calling convention, and parameter sizes.
3. Validate the returned interface vtable before calling through it.
4. Check entity-index conventions separately from player-slot conventions.
5. Call the original hook before modifying a command it owns.
6. Print the resolved interface and module bases in the debug dump.
7. Build for the target's architecture and perform a permitted local smoke
   test before trusting runtime values.

## Current boundary

The project no longer depends on the old entity-list and force-input globals.
Client-only entity fields such as dormancy, absolute origin, and bone-matrix
state still require their own layout, interface, function, or signature
evidence. Server-side globals should only be reintroduced when a feature needs
them and their actual consumers have been documented.
