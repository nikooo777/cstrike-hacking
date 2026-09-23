#!/usr/bin/env python3
"""Compare the vtable slots the DLL calls between two game builds.

Each side is a CS:S install directory or a flat directory of DLLs, such as the
binaries exported from the tutorial's Ghidra project (aidocs/004 section 11.1).

Usage:
  tools/compare_builds.py OLD NEW
  tools/compare_builds.py OLD NEW --locate client.dll:0x152290 --locate engine.dll:0xA0DF0
"""

import argparse
import sys

sys.dont_write_bytecode = True
from check_signatures import module_path  # noqa: E402
from pe import Image, locate, similarity  # noqa: E402

# (module, class, subobject offset, slots the DLL calls)
USED_SLOTS = [
    ("engine.dll", "CEngineClient", 0x0, [19, 20]),
    ("engine.dll", "CVRenderView", 0x0, [50]),
    ("engine.dll", "CModelInfoClient", 0x0, [28]),
    ("engine.dll", "CEngineTraceClient", 0x0, [4]),
    ("engine.dll", "CEngineVGui", 0x0, [35]),
    ("vguimatsurface.dll", "CMatSystemSurface", 0x0, [51, 61, 62, 93, 104]),
    ("client.dll", "CHLClient", 0x0, [8, 14, 15, 35]),
    ("client.dll", "CClientEntityList", 0x40028, [3, 4]),
    ("client.dll", "ClientModeCSNormal", 0x0, [16, 21]),
    ("client.dll", "C_CSPlayer", 0x0, [4, 143]),
    ("client.dll", "C_CSPlayer", 0x8, [9]),
    ("client.dll", "C_CSPlayer", 0x10, [8]),
    ("client.dll", "C_AK47", 0x0, [370, 371, 382, 383, 384]),
    ("client.dll", "CCSGameMovement", 0x0, [15]),
]
MATCH_THRESHOLD = 0.95


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("old", help="last verified build")
    parser.add_argument("new", help="build to check")
    parser.add_argument("--locate", action="append", default=[], metavar="MODULE:RVA",
                        help="also find an old-build function in the new build")
    args = parser.parse_args()

    cache = {}

    def image(side, module):
        key = (side, module)
        if key not in cache:
            cache[key] = Image(module_path(args.old if side == "old" else args.new, module, True))
        return cache[key]

    changed = False
    for module, class_name, subobject, slots in USED_SLOTS:
        old, new = image("old", module), image("new", module)
        old_vtable = old.vtables(class_name).get(subobject)
        new_vtable = new.vtables(class_name).get(subobject)
        label = f"{module}:{class_name}+{subobject:#x}"
        if old_vtable is None or new_vtable is None:
            print(f"{label}: vtable not found (old={old_vtable} new={new_vtable})")
            changed = True
            continue
        old_count, new_count = old.slot_count(old_vtable), new.slot_count(new_vtable)
        print(f"{label}: slots old={old_count} new={new_count}")
        changed |= old_count != new_count
        for slot in slots:
            old_rva = old.vtable_entry(old_vtable, slot)
            new_rva = new.vtable_entry(new_vtable, slot)
            score = similarity(old, old_rva, new, new_rva)
            changed |= score < MATCH_THRESHOLD
            flag = "" if score >= MATCH_THRESHOLD else "  CHANGED"
            print(f"    slot {slot:3}: old=+{old_rva:#x} new=+{new_rva:#x} match={score:.2f}{flag}")

    for request in args.locate:
        module, rva_text = request.split(":")
        old_rva = int(rva_text, 16)
        hits = locate(image("old", module), old_rva, image("new", module))
        if len(hits) == 1:
            score = similarity(image("old", module), old_rva, image("new", module), hits[0])
            print(f"locate {module}+{old_rva:#x} -> +{hits[0]:#x} match={score:.2f}")
        else:
            changed = True
            print(f"locate {module}+{old_rva:#x} -> {len(hits)} candidates")
    return 1 if changed else 0


if __name__ == "__main__":
    sys.exit(main())
