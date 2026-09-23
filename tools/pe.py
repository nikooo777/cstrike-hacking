"""Minimal PE64 reader shared by the build-comparison tools (stdlib only)."""

import re
import struct

SLOT_MASK_WINDOW = 256


class Image:
    def __init__(self, path):
        with open(path, "rb") as handle:
            self.data = handle.read()
        pe = struct.unpack_from("<I", self.data, 0x3C)[0]
        count = struct.unpack_from("<H", self.data, pe + 6)[0]
        optional_size = struct.unpack_from("<H", self.data, pe + 20)[0]
        self.base = struct.unpack_from("<Q", self.data, pe + 24 + 24)[0]
        self.sections = []
        for index in range(count):
            name, virtual_size, rva, raw_size, raw = struct.unpack_from(
                "<8sIIII", self.data, pe + 24 + optional_size + index * 40)
            self.sections.append((name.rstrip(b"\0").decode(), rva,
                                  max(virtual_size, raw_size), raw, raw_size))
        self.text = next(s for s in self.sections if s[0] == ".text")

    def offset(self, rva):
        for _, start, _, raw, raw_size in self.sections:
            if start <= rva < start + raw_size:
                return rva - start + raw
        return None

    def rva(self, offset):
        for _, start, _, raw, raw_size in self.sections:
            if raw <= offset < raw + raw_size:
                return start + offset - raw
        return None

    def read(self, rva, size):
        offset = self.offset(rva)
        return None if offset is None else self.data[offset:offset + size]

    def u64(self, rva):
        chunk = self.read(rva, 8)
        return None if chunk is None or len(chunk) < 8 else struct.unpack("<Q", chunk)[0]

    def rip(self, instruction_rva, displacement_offset, length):
        displacement = struct.unpack("<i", self.read(instruction_rva + displacement_offset, 4))[0]
        return instruction_rva + length + displacement

    def section_of(self, rva):
        return next((s[0] for s in self.sections if s[1] <= rva < s[1] + s[2]), None)

    def in_text(self, rva):
        _, start, size, _, _ = self.text
        return rva is not None and start <= rva < start + size

    def rip_refs(self, target):
        _, start, _, raw, raw_size = self.text
        hits = []
        for offset in range(raw, raw + raw_size - 4):
            displacement = struct.unpack_from("<i", self.data, offset)[0]
            if start + (offset - raw) + 4 + displacement == target:
                hits.append(start + (offset - raw))
        return hits

    def find(self, pattern):
        """Return every .text RVA matching a `48 8B ?? ...` pattern."""
        regex = b"".join(b"." if token.startswith("?") else re.escape(bytes([int(token, 16)]))
                         for token in pattern.split())
        _, start, _, raw, raw_size = self.text
        return [start + match.start()
                for match in re.finditer(regex, self.data[raw:raw + raw_size], re.DOTALL)]

    def vtables(self, class_name):
        """Map subobject offset -> vtable RVA through MSVC RTTI."""
        name_offset = self.data.find((".?AV" + class_name + "@@").encode() + b"\0")
        if name_offset < 0:
            return {}
        type_descriptor = self.rva(name_offset) - 0x10
        found = {}
        for match in re.finditer(re.escape(struct.pack("<I", type_descriptor)), self.data):
            locator = match.start() - 12
            signature, subobject, _ = struct.unpack_from("<III", self.data, locator)
            if signature != 1:
                continue
            locator_pointer = struct.pack("<Q", self.base + self.rva(locator))
            for pointer in re.finditer(re.escape(locator_pointer), self.data):
                found[subobject] = self.rva(pointer.start()) + 8
        return found

    def vtable_entry(self, vtable, slot):
        value = self.u64(vtable + slot * 8)
        return None if value is None else value - self.base

    def slot_count(self, vtable):
        count = 0
        while self.in_text(self.vtable_entry(vtable, count)):
            count += 1
        return count


def masked(image, rva, size=SLOT_MASK_WINDOW):
    """Code bytes with call, jump, and RIP-relative displacements zeroed."""
    code = bytearray(image.read(rva, size))
    keep = [True] * len(code)
    index = 0
    while index < len(code):
        byte = code[index]
        if byte in (0xE8, 0xE9):
            for k in range(index + 1, min(index + 5, len(code))):
                keep[k] = False
            index += 5
            continue
        if byte == 0x0F and index + 1 < len(code) and 0x80 <= code[index + 1] <= 0x8F:
            for k in range(index + 2, min(index + 6, len(code))):
                keep[k] = False
            index += 6
            continue
        if index + 1 < len(code) and (code[index + 1] & 0xC7) == 0x05 and byte != 0x00:
            for k in range(index + 2, min(index + 6, len(code))):
                keep[k] = False
            index += 2
            continue
        index += 1
    return bytes(c if k else 0 for c, k in zip(code, keep)), keep


def similarity(old_image, old_rva, new_image, new_rva, size=SLOT_MASK_WINDOW):
    old_code, _ = masked(old_image, old_rva, size)
    new_code, _ = masked(new_image, new_rva, size)
    return sum(1 for a, b in zip(old_code, new_code) if a == b) / size


def locate(old_image, old_rva, new_image, size=40):
    """Find an old function in a new build by its masked opening bytes."""
    raw = old_image.read(old_rva, size)
    _, keep = masked(old_image, old_rva, size)
    regex = b"".join(re.escape(bytes([byte])) if k else b"." for byte, k in zip(raw, keep))
    _, start, _, text_raw, raw_size = new_image.text
    return [start + match.start()
            for match in re.finditer(regex, new_image.data[text_raw:text_raw + raw_size], re.DOTALL)]
