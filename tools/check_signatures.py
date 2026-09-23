#!/usr/bin/env python3
"""Check a signature profile against a set of game binaries.

Usage:
  tools/check_signatures.py config/signatures-x64.ini "<CS:S install dir>"
  tools/check_signatures.py config/signatures-x64.ini <dir with client.dll ...>
"""

import argparse
import configparser
import os
import struct
import sys

sys.dont_write_bytecode = True
from pe import Image  # noqa: E402

X64_LAYOUT = {
    "client.dll": "cstrike/bin/x64/client.dll",
    "server.dll": "cstrike/bin/x64/server.dll",
    "engine.dll": "bin/x64/engine.dll",
    "vguimatsurface.dll": "bin/x64/vguimatsurface.dll",
}
X86_LAYOUT = {
    "client.dll": "cstrike/bin/client.dll",
    "server.dll": "cstrike/bin/server.dll",
    "engine.dll": "bin/engine.dll",
    "vguimatsurface.dll": "bin/vguimatsurface.dll",
}


def module_path(root, module, x64):
    flat = os.path.join(root, module)
    if os.path.isfile(flat):
        return flat
    layout = X64_LAYOUT if x64 else X86_LAYOUT
    return os.path.join(root, layout.get(module, module))


def decode(image, match, fields):
    operand = fields.get("operand", "match").lower()
    operand_offset = int(fields.get("operand_offset", "0"))
    if operand == "match":
        return match
    if operand == "abs32":
        value = struct.unpack("<I", image.read(match + operand_offset, 4))[0]
        return value - image.base
    if operand == "rip_rel32":
        instruction = match + int(fields.get("instruction_offset", "0"))
        return image.rip(instruction, operand_offset - int(fields.get("instruction_offset", "0")),
                         int(fields["instruction_length"]))
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("profile", help="signature profile, e.g. config/signatures-x64.ini")
    parser.add_argument("binaries", help="CS:S install directory or a flat directory of DLLs")
    args = parser.parse_args()

    ini = configparser.ConfigParser(interpolation=None, strict=False)
    ini.read(args.profile)
    x64 = "x64" in os.path.basename(args.profile)
    images = {}
    failed = False
    for section in ini.sections():
        if not section.startswith("signature."):
            continue
        fields = ini[section]
        module = fields["module"]
        if module not in images:
            images[module] = Image(module_path(args.binaries, module, x64))
        image = images[module]
        matches = image.find(fields["pattern"])
        required = fields.get("required", "false").lower() == "true"
        line = f"{section[len('signature.'):]:24} {module:12} matches={len(matches)}"
        if len(matches) == 1:
            line += f" at +{matches[0]:#x}"
            target = decode(image, matches[0], fields)
            if target is not None and target != matches[0]:
                line += f" -> +{target:#x}"
                if fields.get("indirections", "0") == "1":
                    line += f" (slot in {image.section_of(target)}, {len(image.rip_refs(target))} code refs)"
        elif required:
            failed = True
            line += "  REQUIRED SIGNATURE FAILED"
        print(line)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
