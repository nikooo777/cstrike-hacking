# Netvars and entity offsets

This note follows the signature-scanning guide. It answers a practical
question: when an entity field is replicated by Source, can we discover its
offset from the client's receive metadata instead of maintaining a hardcoded
`this + 0x...` value?

The answer is usually yes. A netvar is still an entity-relative offset, but it
is derived at runtime from the field's semantic name and its `RecvTable` path.
It is not a module address, a signature, or a guarantee that the field is safe
to read or write.

Use this only with binaries and processes you are permitted to study,
preferably in an offline or local test environment.

## The mental model

The relevant metadata path is:

~~~text
BaseClient::GetAllClasses()
  -> ClientClass linked list
       -> root RecvTable (for example DT_BasePlayer)
            -> RecvProp name + displacement
                 -> nested data table(s)
                      -> entity-relative offset
~~~

`ClientClass` identifies a client network class and points at its receive
table. A `RecvTable` owns an array of `RecvProp` records. A property can be a
scalar field, an array element, or another data table. For a nested property,
the effective offset is the sum of each parent data-table displacement and the
final property's displacement.

The layout used here comes from the 32-bit Source 1 ABI. Compare the local
target with the [Valve `ClientClass` definition](https://raw.githubusercontent.com/ValveSoftware/source-sdk-2013/master/src/public/client_class.h),
the [Valve `RecvProp`/`RecvTable` definitions](https://raw.githubusercontent.com/ValveSoftware/source-sdk-2013/master/src/public/dt_recv.h),
and the [Valve `IBaseClientDLL` interface](https://raw.githubusercontent.com/ValveSoftware/source-sdk-2013/master/src/public/cdll_int.h).
Those references explain the field order; they do not make an unrelated game
build compatible.

## What changed in this project

`Nikooo777/netvars/netvars.cpp` now:

1. Calls `BaseClient::GetAllClasses()` after `VClient017` is resolved.
2. Walks the linked list and recursively indexes named receive tables,
   including nested tables such as `DT_Local`.
3. Validates readable metadata, bounded property counts, cycles, and offset
   ranges before using it.
4. Resolves required fields by table and property name.
5. Caches the result for `DEFINE_NETVAR` accessors.

Startup fails before hooks are enabled if a required table or property is
missing. That is intentional: using a stale hardcoded offset is worse than
refusing to start.

The accessor form is deliberately small:

~~~cpp
DEFINE_NETVAR(Vector3, m_vecOrigin, "DT_BaseEntity", "m_vecOrigin");
~~~

The method computes the address as `entity + netvars::GetOffset(...)`. It must
only be called after netvar initialization and on the entity type for which the
table path was verified.

Source often spells vector elements as `m_vecVelocity[0]`. The resolver accepts
an exact property name and the common `[0]` spelling, so the code can keep the
semantic name `m_vecVelocity` while matching either representation.

## Fields migrated in this tutorial

| Accessor | Receive-table path | Why it belongs here |
|---|---|---|
| `CBasePlayer::m_lifeState()` | `DT_BasePlayer -> m_lifeState` | Replicated player state. |
| `CBasePlayer::m_iHealth()` | `DT_BasePlayer -> m_iHealth` | Replicated player state. |
| `CBasePlayer::m_iTeamNum()` | `DT_BaseEntity -> m_iTeamNum` | Replicated base-entity state. |
| `CBasePlayer::m_vecOrigin()` | `DT_BaseEntity -> m_vecOrigin` | Replicated entity position. |
| `CBasePlayer::m_vecViewOffset()` | `DT_LocalPlayerExclusive -> m_vecViewOffset[0]` | Replicated player view position; `[0]` is accepted by the resolver. |
| `CBasePlayer::m_vecVelocity()` | `DT_LocalPlayerExclusive -> m_vecVelocity[0]` | Replicated movement state. |
| `CBasePlayer::m_vecBaseVelocity()` | `DT_LocalPlayerExclusive -> m_vecBaseVelocity` | Replicated base-velocity state. |
| `CBasePlayer::m_angRotation()` | `DT_BaseEntity -> m_angRotation` | Replicated entity rotation. |
| `CBasePlayer::m_fFlags()` | `DT_BasePlayer -> m_fFlags` | Replicated movement flags. |
| `CBasePlayer::m_Local()` | `DT_LocalPlayerExclusive -> m_Local` | Nested `CPlayerLocalData` table. |
| `CLocal::m_vecPunchAngle()` | `DT_Local -> m_vecPunchAngle` | Resolved relative to the `m_Local` subobject. |
| `CLocal::m_vecPunchAngleVel()` | `DT_Local -> m_vecPunchAngleVel` | Resolved relative to the `m_Local` subobject. |
| `CCSPlayer::m_iShotsFired()` | `DT_CSLocalPlayerExclusive -> m_iShotsFired` | Game-specific replicated player state. |

The crosshair target is intentionally not in this table. Neither architecture
exposes it as a RecvProp. Ghidra shows the x86 client reading the client-only
field at `player + 0x14F0`; chapter 004 records the x64
`C_CSPlayer::GetIDTarget` path at `player + 0x1B20`. The two offsets remain
architecture- and build-specific.

## Ghidra evidence for the current sample

The table names above were checked against the Ghidra project for the current
`client.dll` before finalizing the accessors. These are sample-specific anchors,
not offsets to copy into a different build:

| Ghidra table-builder | Table name | Entries visible in the builder |
|---|---|---|
| `FUN_100a3740` | `DT_BaseEntity` | `m_iTeamNum` `0x9C`, `m_vecOrigin` `0x338`, `m_angRotation` `0x344` |
| `FUN_100b28d0` | `DT_BasePlayer` | `m_lifeState` `0x93`, `m_iHealth` `0x94`, `m_fFlags` `0x350` |
| `FUN_100b30f0` | `DT_LocalPlayerExclusive` | `m_vecViewOffset[0]` `0xE8`, `m_vecVelocity[0]` `0xF4`, `m_Local` `0xDDC`, `m_vecBaseVelocity` `0x158` |
| `FUN_100b2b80` | `DT_Local` | `m_vecPunchAngle` `0x70`, `m_vecPunchAngleVel` `0xAC` |
| `FUN_1004bbf0` | `DT_CSLocalPlayerExclusive` | `m_iShotsFired` `0x1540` |

For example, decompiling `FUN_1004bbf0` shows the property name
`m_iShotsFired`, the displacement `0x1540`, and the final table-name pointer
to the string `DT_CSLocalPlayerExclusive`. That is why the semantic path is
not guessed from the broader `DT_CSPlayer` class name.

The same inspection found no `m_iCrosshairID` RecvProp string. An instruction
search found client writes using `[this + 0x14F0]`, which is useful evidence for
the current client-only accessor but does not make that field a netvar.

The durable workflow is therefore: locate the table-builder or metadata string
in Ghidra, verify the property name and displacement, then confirm the live
`GetAllClasses()` graph in the target process. The live graph remains the
runtime source of truth; Ghidra is the cross-check that explains how to find
the table and why the chosen semantic path is credible.

The aimbot's `m_dwBoneMatrix` remains a `DEFINE_MEMBER` field. A bone-matrix
pointer is client-side implementation state, not automatically a networked
property. Likewise, `m_bDormant` is client networkable state rather than a
normal RecvProp, and `m_vecAbsOrigin` is a client-derived transform. They need
different evidence: an interface, a signature, a verified client layout, or a
game function.

The entity-list and force-input values are not netvars. They now use the
named client entity-list interface and `CUserCmd::buttons`; the discovery and
ABI checks are documented in [003 - Global addresses, interfaces, and input
commands](003_global-addresses-and-inputs.md).

## How to discover a new netvar

When adding a field, keep the discovery work separate from the accessor edit:

1. **Identify the object.** Confirm that the pointer is a client entity and
   determine its class family. Do not use a `DT_CSPlayer` path on an arbitrary
   `CBaseEntity` without checking the runtime type.
2. **Find the metadata.** In the permitted target, inspect the `ClientClass`
   list and the `RecvTable` graph. A small debugger script or a temporary
   diagnostic dump can print table names, property names, and raw displacements.
   This project prints the fields it requires in the F1 debug dump; extending
   it with a full table dump is useful when exploring a new field.
3. **Use Ghidra for confirmation.** If the table name or ABI is uncertain,
   inspect the client code that registers/decodes the table and compare the
   structure field order with the Source SDK references above. Ghidra confirms
   what the metadata pointer means; it does not replace reading the actual
   table in the target process.
4. **Follow nested tables.** If a property is under `m_Local` or another data
   table, add every parent displacement. A resolver that only searches the
   first table will return a relative offset or miss the property entirely.
5. **Check the type and semantics.** A matching name is not enough. Confirm
   whether the value is an `int`, `float`, vector, handle, array, or boolean,
   and whether the client actually receives it for the entity class in use.
6. **Compare evidence.** Compare the resolved offset with the existing layout,
   a second known build, or a read-only debugger observation. A matching old
   constant is useful validation, not a reason to skip the metadata check.
7. **Add the contract.** Add the `DEFINE_NETVAR` accessor, include the table
   and property path in this document or a follow-up note, and add it to the
   required list if the feature cannot safely run without it.

The durable record is the semantic path, not just the resulting hex number:

~~~text
DT_BasePlayer -> m_Local -> DT_Local -> m_vecPunchAngle
~~~

That path tells the next reader where the value came from and how to re-check
it after an update.

## Netvars versus signatures and constants

Choose the source of truth that matches the kind of value:

| Value | Preferred source |
|---|---|
| Replicated entity field | `RecvTable`/netvar path. |
| Global entity-list or input command slot | Named interface or `CUserCmd`; use a module-relative signature only when no semantic source exists. |
| Interface pointer or client-state global | `CreateInterface` or a stable signature with operand decoding. |
| Virtual function entry | Known interface/vtable slot, verified for the target ABI. |
| Client-only state or derived transform | Client layout/function/signature evidence; not a netvar by default. |
| Bone-matrix pointer | Verified client-side layout or a build-specific discovery method. |

Netvars are not magic version independence. Their metadata ABI is
architecture-specific: x64 widens pointer fields and moves members inside
`ClientClass`, `RecvTable`, and `RecvProp`. The target can also rename, remove,
or reorganize properties. Netvars remain valuable because they preserve the
engine's semantic field names and parent structure, which is generally more
maintainable than scattering numbers through feature code. Chapter 004 lists
the verified x86/x64 metadata offsets and compile-time assertions.

The checked-in `config/signatures.ini` remains the right place for byte
patterns, operand rules, module names, and runtime feature settings. Netvar
paths are kept beside the accessors because they describe the SDK-facing type
contract; the runtime offset is still read from the loaded client. If support
for multiple unrelated Source builds is added later, the paths can be made
configurable, but startup validation must remain mandatory.

## Troubleshooting

### `GetAllClasses returned null`

The BaseClient interface is unavailable, the interface slot is wrong for the
target, or initialization ran before the client was ready. Verify `VClient017`
and the `GetAllClasses` vtable position before debugging individual fields.

### `missing RecvTable ...` or `missing netvar ...`

Check the exact spelling and table family. A table may be nested rather than a
`ClientClass` root, which is why this implementation indexes the complete
reachable graph. If the property was removed or renamed, update the semantic
path only after confirming the new field in the target.

### The offset is present but the value is implausible

Check that the pointer is the correct entity type, the field type matches the
accessor, and the base offset is the correct object. For nested fields, make
sure the accessor is called on the nested object when the API intentionally
returns a relative offset, as `CLocal::m_vecPunchAngle()` does.

### Why does the program stop instead of trying the old offset?

Because a partial fallback would hide a build mismatch. If a compatibility
fallback is ever needed, it should be an explicit versioned path with its own
validation and diagnostics, not a silent return to an unverified constant.

## Current boundary

This change migrates the fields used by the player/aim/trigger/no-recoil
experiments that are represented by receive tables. It does not claim that all
fields in `sdk/entity/` are networked or that all remaining constants can be
replaced with netvars. Treat each remaining member as a separate reverse-
engineering question, and preserve its provenance in the numbered tutorial
notes.
