#!/usr/bin/env python3
"""Reduce a Doom IWAD to fit the xBuddy internal flash (GBADoom needs the WAD
memory-mapped). Drops content our build never uses: sounds, music, demos, and
all maps except a keep-list. Sprites/textures/flats/UI are kept (cutting those
risks engine crashes; do that only if size still doesn't fit).

Usage: reduce_wad.py <in.wad> <out.wad> [KEEPMAP ...]   (default keepmap: E1M1)
"""
import sys, struct, re

MAP_SUBLUMPS = {b"THINGS", b"LINEDEFS", b"SIDEDEFS", b"VERTEXES", b"SEGS",
                b"SSECTORS", b"NODES", b"SECTORS", b"REJECT", b"BLOCKMAP"}
MAP_MARKER = re.compile(rb"^E\dM\d$|^MAP\d\d$")

def name8(b):
    return b.split(b"\0", 1)[0]

def main():
    src, dst = sys.argv[1], sys.argv[2]
    keep_maps = {m.encode() for m in (sys.argv[3:] or ["E1M1"])}

    data = open(src, "rb").read()
    magic, numlumps, infotab = struct.unpack_from("<4sii", data, 0)
    assert magic in (b"IWAD", b"PWAD"), magic
    dirents = []
    for i in range(numlumps):
        pos, size, raw = struct.unpack_from("<ii8s", data, infotab + i * 16)
        dirents.append((pos, size, raw, name8(raw)))

    def drop(nm):
        if nm in (b"ENDOOM", b"GENMIDI", b"DMXGUS", b"DMXGUSC"):
            return True
        if nm.startswith(b"DEMO"):
            return True
        if nm.startswith(b"DS") or nm.startswith(b"DP"):   # sound effects
            return True
        if nm.startswith(b"D_"):                            # music
            return True
        return False

    keep = []
    i = 0
    while i < len(dirents):
        pos, size, raw, nm = dirents[i]
        if MAP_MARKER.match(nm):
            grp = [dirents[i]]
            j = i + 1
            while j < len(dirents) and dirents[j][3] in MAP_SUBLUMPS:
                grp.append(dirents[j]); j += 1
            if nm in keep_maps:
                keep.extend(grp)
            i = j
            continue
        if not drop(nm):
            keep.append(dirents[i])
        i += 1

    # write new WAD
    out = bytearray(12)
    new_dir = []
    for pos, size, raw, nm in keep:
        lump = data[pos:pos + size]
        new_dir.append((len(out), size, raw))
        out += lump
    infotab_new = len(out)
    for pos, size, raw in new_dir:
        out += struct.pack("<ii8s", pos, size, raw)
    struct.pack_into("<4sii", out, 0, magic, len(new_dir), infotab_new)
    open(dst, "wb").write(out)

    print(f"in:  {len(data):>9} bytes, {numlumps} lumps")
    print(f"out: {len(out):>9} bytes, {len(new_dir)} lumps  ({dst})")

if __name__ == "__main__":
    main()
