# Signature scanning, offsets, and pointer derivation

This is the next step after locating the client interfaces: learn how to find
the same code or data again when a module is loaded at a different address or
when a small update moves everything around.

The project is a 32-bit, build-specific Counter-Strike: Source experiment.
Use these techniques only against binaries and processes you are permitted to
study, preferably in an offline or local test environment. A successful scan
is not evidence that a pointer is safe to read or write.

## The important mental model

A signature is a locator, not a value and not a type. The scan gives us a
location in code. The instructions at that location tell us what the location
means and how to derive the pointer, function, or field offset we actually
need.

The complete workflow is:

~~~text
choose a semantic code site
  -> select stable instruction bytes
  -> wildcard relocatable operands
  -> scan the intended module
  -> require one match
  -> disassemble and decode the operand
  -> validate the resolved address and object
~~~

Do not collapse all of those steps into “the pattern found an offset”. There
are several different addresses involved:

| Thing | Example | How it is obtained |
| --- | --- | --- |
| Module-relative offset | `client.dll + 0x4D5AE4` | Add a build-specific offset to the module base. |
| Signature match | `client.dll + 0xE6CA3` | Scan the module for a byte sequence. |
| Operand field | `match + 2` for `8B 0D imm32` | Point at the four bytes encoded inside an instruction. |
| Absolute pointer value | `0x10525764` in the sample | Decode the operand bytes as a little-endian x86 address. |
| Resolved object pointer | `ClientMode *` | Dereference the global slot or otherwise follow the instruction semantics. |
| Object field offset | `[ecx + 0xXYZ]` | Read the displacement from an object access; do not add it to a module base. |

The distinction between the match, the operand, and the final pointer is the
part that prevents most signature-scanning mistakes.

## What the project has today

The direct constants in `Nikooo777/core/offsets.h` are module-relative
addresses for one client build:

~~~cpp
auto entityListSlot = core::GetModule("client.dll") + dwEntityList;
auto forceJump = core::GetModule("client.dll") + dwForceJump;
~~~

The entity-list and force-command values are not absolute process addresses.
They only become addresses after adding the correct module base. The
`dwNumPlayers` and `dwMaxPlayers` values are consumed with `server.dll` in
`game/entity_list.cpp`, so the module must be part of the record for every
offset.

The interface resolver in `Nikooo777/game/interfaces.cpp` already demonstrates
the other approach:

~~~cpp
// ClientState code site in engine.dll.
"B9 ? ? ? ? E8 ? ? ? ? FF 75 FC E8 ? ? ? ? 83"

// ClientMode code site in client.dll.
"8B 0D ? ? ? ? 8B 01 5D FF 60 28 CC"
~~~

`mem::ScanModComboAll` returns every match. `mem::ScanModComboUnique` returns
a pointer only when exactly one match exists and reports the count through its
out parameter. `ScanModCombo` remains a legacy first-match helper; interface
resolution uses the unique variant.

## Choosing a stable signature

Start from the thing you want to recover, then work backwards:

1. Find a code path that accesses the value.
2. Identify the instruction whose operand contains the useful address or
   displacement.
3. Include enough surrounding instructions to make the site unique.
4. Wildcard bytes that are expected to move between builds.
5. Check the pattern against the entire intended module.
6. Confirm that the result is unique and that the decoded value has the
   expected meaning.

Prefer bytes that describe stable program structure:

- opcodes and ModR/M bytes that express the same operation;
- a short sequence crossing several related instructions;
- a nearby vtable dispatch or other semantic anchor;
- control-flow structure that is unlikely to be emitted in many places.

Usually wildcard values that are tied to a particular build:

- absolute addresses embedded in `mov`, `push`, or memory operands;
- the four-byte displacement in a relative `call` or `jmp`;
- pointers to globals, strings, or vtables;
- compiler-generated addresses and constants that are not part of the
  behavior being identified.

Do not wildcard everything. A pattern made almost entirely of question marks
will match unrelated code. Do not rely on one arbitrary function-prologue
sequence either; compiler settings can change prologues while leaving the
interesting access intact. Select complete instructions and keep the
signature as short as possible while still unique.

“Stable” does not mean permanent. It means that the chosen bytes describe a
semantic site more reliably than a hardcoded address. Test a candidate against
at least two known binaries when possible.

## Ghidra workflow

Use Ghidra to understand the instruction semantics before writing the runtime
resolver:

1. Import the exact module that the running process will load.
2. Search for a candidate byte sequence in the relevant module.
3. Open the result and disassemble from the match address.
4. Check instruction boundaries and identify which bytes are operands.
5. Inspect xrefs, nearby code, strings, and vtable usage to confirm the site.
6. Record the image base and the module-relative match offset.
7. Repeat the search on another build if one is available.

The Ghidra MCP byte-search endpoint accepts `??` for a wildcard byte. The
runtime scanner's combo strings in this repository use a single `?` token:

~~~text
Ghidra search:  8B 0D ?? ?? ?? ?? 8B 01 5D FF 60 28 CC
Runtime scan:   8B 0D ? ? ? ? 8B 01 5D FF 60 28 CC
~~~

That is a difference in search-tool syntax, not a difference in the bytes
that should be considered stable.

## Keeping provenance with the pattern

`config/signatures.ini` is the runtime source of truth. It keeps the bytes next to the reasoning that made them trustworthy, so a future update has a clear way to re-derive the record.

A signature record should answer four questions:

1. Which module owns the code site?
2. Which bytes are stable, and which operand bytes are wildcarded?
3. How does the instruction turn the match into the value we need?
4. Where can someone review the disassembly and the discovery process?

For example, the ClientMode record is intentionally more descriptive than a byte string:

~~~ini
[signature.ClientMode]
module=client.dll
pattern=8B 0D ? ? ? ? 8B 01 5D FF 60 28 CC
operand=abs32
operand_offset=2
indirections=1
source=aidocs/001_signature-scanning-and-offsets.md
source_readme=README.md#tutorial-notes
discovery=Confirm the global load, the ClientMode dereference, and the vtable dispatch in Ghidra.
~~~

`operand_offset` points at the encoded operand inside the instruction. `indirections` records how many pointer reads the instruction semantics require after decoding it. Those fields prevent the common mistake of treating the address of the operand bytes as the final object.

When adding a signature for a new build:

1. Find the semantic code site in Ghidra and verify it with xrefs or surrounding control flow.
2. Copy complete instructions, wildcarding absolute addresses and relative displacements.
3. Search the intended module and record the match count and module-relative match offset.
4. Decode the operand according to the instruction encoding, then validate the resulting pointer or vtable.
5. Add the pattern, operand rule, and provenance fields to `config/signatures.ini`.
6. Put the durable explanation in the numbered `aidocs/` note and link any relevant reversing video from `README.md`.

CMake copies the config beside the DLL. At startup, the resolver logs the loaded path, match offset, and provenance fields, which makes the runtime result auditable without turning a one-build address into a permanent constant.

## Worked example: resolving ClientMode

For the client.dll sample loaded into Ghidra for this tutorial:

- image base: `0x10000000`;
- one match for the ClientMode pattern: `0x100E6CA3`;
- module-relative match: `client.dll + 0xE6CA3`;
- bytes at the match:
  `8B 0D 64 57 52 10 8B 01 5D FF 60 28 CC`.

Ghidra disassembles the beginning as:

~~~text
100E6CA3  MOV ECX, dword ptr [0x10525764]   8B 0D 64 57 52 10
100E6CA9  MOV EAX, dword ptr [ECX]           8B 01
100E6CAB  POP EBP                            5D
100E6CAC  JMP dword ptr [EAX + 0x28]         FF 60 28
~~~

The first instruction is the key:

~~~text
8B 0D imm32
     ^^^^^
     four-byte absolute address of a global slot
~~~

The pointer chain is therefore:

~~~text
match
  + 2
    -> address of the encoded imm32 field
decode imm32
    -> address of the global ClientMode* slot
read the global slot
    -> ClientMode*
~~~

In pseudocode, the complete x86 resolution is:

~~~cpp
std::uint32_t encodedSlot = 0;
std::memcpy(&encodedSlot, match + 2, sizeof encodedSlot);

auto globalSlot = static_cast<std::uintptr_t>(encodedSlot);
auto clientMode = *reinterpret_cast<ClientMode **>(globalSlot);
~~~

The address `0x10525764` is an observation from this particular Ghidra
sample. It must not be copied into `core/offsets.h`; the runtime code should
decode the four operand bytes at `match + 2`.

### Pointer-chain review

Before this change, `GetClientMode()` did this:

~~~cpp
auto clientModePtrAddr = reinterpret_cast<std::uintptr_t>(scan) + 2;
g_clientMode = *reinterpret_cast<ClientMode **>(clientModePtrAddr);
~~~

For the exact `8B 0D imm32` encoding shown above, that single dereference reads
the encoded `0x10525764` value from the instruction stream. It does not read
the `ClientMode*` stored at the global slot.

The implementation now decodes the operand and then reads the global slot:

~~~cpp
std::uintptr_t globalSlot = 0;
if (!mem::DecodeAbs32(scan, 2, globalSlot) || globalSlot == 0) {
    return nullptr;
}
auto *clientMode = mem::ReadPointer<ClientMode>(
    reinterpret_cast<const void *>(globalSlot));
~~~

It also checks the object vtable and its first executable function before the
pointer is cached. A successful build still does not prove that a signature
survives a different module build.

## ClientState is a different instruction shape

The current engine signature begins with:

~~~text
B9 imm32
~~~

`B9` is `mov ecx, imm32`: it loads the immediate value itself into ECX. If the
immediate names the ClientState object, the operand is resolved with one decode
and no extra global-slot dereference:

~~~cpp
std::uint32_t encodedState = 0;
std::memcpy(&encodedState, scan + 1, sizeof encodedState);
auto clientState =
    reinterpret_cast<ClientState *>(static_cast<std::uintptr_t>(encodedState));
~~~

Whether the immediate is the final object, a pointer slot, or a helper address
must be confirmed from the surrounding instructions. Never infer that from
the fact that another signature also uses `match + 1` or `match + 2`.

The resolver now stores the decoded ClientState object address in
`g_clientStateAddr`. Debug output reports that address and its module-relative
value only when both are valid; it no longer treats `scan + 1` (the operand
field) as the object address.

## Other common operand shapes

The instruction form determines the decoding formula.

### Absolute memory operand

For `8B 0D imm32`, the immediate is the address of a memory slot:

~~~text
slot = read_u32(match + 2)
value = read_pointer(slot)
~~~

### Immediate pointer

For `B9 imm32`, the immediate may be the final pointer:

~~~text
value = pointer_from_u32(read_u32(match + 1))
~~~

Confirm the surrounding code before treating it as final.

### Relative call or jump

For an x86 `E8 rel32` call, the displacement is relative to the next
instruction:

~~~text
target = match + 5 + sign_extended(read_i32(match + 1))
~~~

Wildcard the four-byte displacement, but keep the `E8` opcode and enough
surrounding instructions to identify the intended call site.

### Object field access

For an instruction such as `mov eax, [ecx + 0xXYZ]`, `0xXYZ` is an offset
inside the object pointed to by ECX. It is not a module-relative offset and
must not be calculated as `moduleBase + 0xXYZ`. The object type and the code
that establishes ECX are part of the evidence for the field.

## Validation before using a result

A resolver should fail closed. Before installing a hook or reading a derived
object, check as many of these as the target permits:

- the module handle and module size are nonzero;
- the signature has exactly one match in the intended module;
- the operand bytes are readable and the decoded value is nonzero;
- a decoded code target falls inside an executable module region;
- a decoded object pointer is aligned and points to readable memory;
- a vtable pointer, when present, points into the expected module;
- the result has the expected x86 shape and nearby xrefs;
- the value is logged as both an absolute address and a module-relative offset.

If a signature has zero matches, stop and report a build mismatch. If it has
multiple matches, do not silently use the first result. Either strengthen the
signature with a semantic neighbor or use the surrounding function/xref
context to select the correct site.

## A practical maintenance loop

For each value that currently lives in `core/offsets.h`:

1. Find a code site that consumes or writes the value.
2. Capture a short, unique sequence of complete instructions.
3. Mark absolute addresses and relative displacements as wildcards.
4. Search the whole owning module and record the match count.
5. Decode the operand according to its instruction encoding.
6. Validate the resulting pointer or field access.
7. Compare the result with a second build before replacing the constant.
8. Keep a debug line containing the module, signature name, match offset, and
   resolved pointer.

Do this one value at a time. A signature-derived address is not automatically
better than a static offset; it is better only when the signature is unique,
the operand is decoded correctly, and the result is validated.

## Current status of this tutorial

The reusable scanner and resolver helpers are now implemented:
- `ScanModComboAll` exposes all matches.
- `ScanModComboUnique` rejects zero or multiple matches.
- `DecodeAbs32`, `ReadPointer`, and readable/executable checks make operand
  decoding and pointer validation explicit.
- ClientState and ClientMode resolution fail closed with diagnostic messages.

The ClientMode byte sequence and instruction interpretation above were checked
against the client.dll sample in the live Ghidra project. The address and
uniqueness result are specific to that sample; they are not a claim of
cross-version compatibility. Entity fields that are part of the client receive
tables are now handled in [002 - Netvars and entity offsets](002_netvars-and-entity-offsets.md).
The entity-list and force-input globals, client-only state, and bone-matrix
layout remain build-specific until they have a different source of truth.
