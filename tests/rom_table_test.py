"""The ROM table is written twice: in C++ (src/runtime/rom_zip.cpp, the game
reading the zip) and in Python (tools/romtool.py, the extraction to data/).
This test checks that both list the same regions, layouts and chips (name,
CRC and offset, in the same order), so a fix in one cannot leave the other
behind and the missing chip message stays true.

Usage: python tests/rom_table_test.py [repository folder]
"""
import importlib.util
import os
import re
import sys


def python_table(root):
    spec = importlib.util.spec_from_file_location("romtool", os.path.join(root, "tools", "romtool.py"))
    romtool = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(romtool)
    return {region: (s["stride"], s["chunk"], s["swap"], [tuple(f) for f in s["files"]])
            for region, s in romtool.REGIONS.items()}


def cpp_table(root):
    with open(os.path.join(root, "src", "runtime", "rom_zip.cpp"), encoding="utf-8") as f:
        source = f.read()
    body = source[source.index("const std::vector<Region>& regions()"):]
    body = body[:body.index("return r;")]
    table = {}
    region = re.compile(r"\{&RomSet::(\w+),\s*(\d+),\s*(\d+),\s*(true|false),")
    chip = re.compile(r'\{"([^"]+)",\s*(0x[0-9A-Fa-f]+),\s*(0x[0-9A-Fa-f]+)\}')
    starts = list(region.finditer(body))
    for i, m in enumerate(starts):   # a region's chips: up to the next region
        chips = body[m.end():starts[i + 1].start() if i + 1 < len(starts) else len(body)]
        files = [(name, int(crc, 16), int(off, 16)) for name, crc, off in chip.findall(chips)]
        table[m.group(1)] = (int(m.group(2)), int(m.group(3)), m.group(4) == "true", files)
    return table


def describe(layout):
    stride, chunk, swap, files = layout
    return f"stride {stride}, chunk {chunk}, swap {swap}; " + ", ".join(f"{n} {c:08x} @{o:x}" for n, c, o in files)


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
    py, cpp = python_table(root), cpp_table(root)
    failed = 0
    if not cpp:
        print("FAILED: no region found in rom_zip.cpp (has the table's layout changed?)")
        return 1
    for region in sorted(set(py) | set(cpp)):
        if region not in cpp:
            print(f"FAILED: {region} is in romtool.py but not in rom_zip.cpp")
        elif region not in py:
            print(f"FAILED: {region} is in rom_zip.cpp but not in romtool.py")
        elif py[region] != cpp[region]:
            print(f"FAILED: {region} differs\n  romtool.py:  {describe(py[region])}\n  rom_zip.cpp: {describe(cpp[region])}")
        else:
            continue
        failed += 1
    chips = sum(len(r[3]) for r in py.values())
    print(f"{len(py)} regions, {chips} chips: " + ("the same in both" if not failed else f"{failed} differ"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
