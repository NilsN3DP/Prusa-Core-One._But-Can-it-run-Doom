#!/usr/bin/env python3
"""Build a minimal, crash-safe single-level (E1M1) Doom IWAD that fits the
xBuddy internal flash for GBADoom.

Strategy (all choices avoid engine I_Error crashes):
  - keep only the E1M1 map; drop other maps, sounds, music, demos
  - keep S_START/S_END but only sprite families E1M1 needs (missing whole sprites
    are skipped gracefully by R_InitSpriteDefs)
  - keep ALL flats (a missing flat referenced by a sector hard-crashes)
  - rewrite TEXTURE1 + PNAMES to ONLY the textures E1M1 references (+SKY1) and drop
    unused patch lumps. Dropped textures => R_CheckTextureNumForName returns -1 =>
    NO_TEXTURE (blank wall), never a crash.
  - drop title/help/credit/end screens (keep TITLEPIC), keep status bar/menu/
    intermission/font/palette.

Usage: reduce_doom.py <in.wad> <out.wad>
"""
import sys, struct, re

KEEP_MAP = b"E1M1"
MAP_SUB = {b"THINGS", b"LINEDEFS", b"SIDEDEFS", b"VERTEXES", b"SEGS",
           b"SSECTORS", b"NODES", b"SECTORS", b"REJECT", b"BLOCKMAP"}
MAP_MARKER = re.compile(rb"^E\dM\d$|^MAP\d\d$")

# Sprite 4-char families needed for E1M1 (monsters present + player + obtainable
# weapons + items + standard effects). Generous: extra ones are harmless.
KEEP_SPR = {b"POSS", b"SPOS", b"TROO", b"PLAY",
    b"PUNG", b"PISG", b"PISF", b"SHTG", b"SHTF", b"CHGG", b"CHGF", b"MISG", b"MISF", b"SAWG",
    b"SHOT", b"MGUN", b"LAUN", b"CLIP", b"SHEL", b"AMMO", b"SBOX", b"BROK", b"ROCK", b"CELL",
    b"BON1", b"BON2", b"ARM1", b"ARM2", b"STIM", b"MEDI", b"BPAK",
    b"CBRA", b"ELEC", b"COLU", b"POL5", b"BAR1", b"CAND",
    b"PUFF", b"BLUD", b"TFOG", b"IFOG", b"MISL", b"BEXP", b"BAL1",
    b"BKEY", b"RKEY", b"YKEY", b"BSKU", b"RSKU", b"YSKU"}

DROP_GFX = {b"HELP", b"HELP1", b"HELP2", b"CREDIT", b"VICTORY2", b"PFUB1", b"PFUB2",
            b"BOSSBACK", b"ENDOOM", b"GENMIDI", b"DMXGUS", b"DMXGUSC"}

def nm8(b): return b.split(b"\0", 1)[0]

def read_wad(path):
    d = open(path, "rb").read()
    magic, n, info = struct.unpack_from("<4sii", d, 0)
    lumps = []
    for i in range(n):
        pos, size, raw = struct.unpack_from("<ii8s", d, info + i * 16)
        lumps.append([nm8(raw), d[pos:pos+size]])
    return magic, lumps

def write_wad(path, magic, lumps):
    out = bytearray(12)
    direc = []
    for name, data in lumps:
        direc.append((len(out), len(data), name.ljust(8, b"\0")[:8]))
        out += data
    info = len(out)
    for pos, size, raw in direc:
        out += struct.pack("<ii8s", pos, size, raw)
    struct.pack_into("<4sii", out, 0, magic, len(direc), info)
    open(path, "wb").write(out)
    return len(out)

def main():
    src, dst = sys.argv[1], sys.argv[2]
    magic, lumps = read_wad(src)
    idx = {name: i for i, (name, _) in enumerate(lumps)}

    # --- collect E1M1 used textures from SIDEDEFS ---
    used_tex = set()
    for i, (name, data) in enumerate(lumps):
        if name == KEEP_MAP:
            for j in range(i + 1, i + 11):
                if lumps[j][0] == b"SIDEDEFS":
                    sd = lumps[j][1]
                    for k in range(len(sd) // 30):
                        for off in (4, 12, 20):  # top, bottom, mid
                            t = nm8(sd[k*30+off:k*30+off+8]).upper()
                            if t and t != b"-":
                                used_tex.add(t)
            break
    used_tex.add(b"SKY1")  # E1 sky

    # --- parse PNAMES + TEXTURE1, rewrite to used textures only ---
    pnames_raw = lumps[idx[b"PNAMES"]][1]
    npn = struct.unpack_from("<i", pnames_raw, 0)[0]
    pnames = [nm8(pnames_raw[4+8*i:4+8*i+8]).upper() for i in range(npn)]

    tex_raw = lumps[idx[b"TEXTURE1"]][1]
    ntex = struct.unpack_from("<i", tex_raw, 0)[0]
    offs = struct.unpack_from("<%di" % ntex, tex_raw, 4)

    new_textures = []   # (name, raw maptexture bytes with remapped patch idx)
    keep_patch_old = set()
    # first pass: find used textures + their patch indices
    for o in offs:
        name = nm8(tex_raw[o:o+8]).upper()
        if name not in used_tex:
            continue
        masked, w, h, coldir, pcount = struct.unpack_from("<ihhih", tex_raw, o+8)
        patches = []
        for p in range(pcount):
            ox, oy, pi, sd_, cm = struct.unpack_from("<hhhhh", tex_raw, o+22+p*10)
            patches.append([ox, oy, pi, sd_, cm])
            keep_patch_old.add(pi)
        new_textures.append((name, masked, w, h, coldir, patches))

    # build compact PNAMES (only referenced patches), old->new index map
    used_pn_idx = sorted(keep_patch_old)
    remap = {old: new for new, old in enumerate(used_pn_idx)}
    new_pnames = [pnames[i] for i in used_pn_idx]

    # serialize new PNAMES
    pn_out = struct.pack("<i", len(new_pnames))
    for nm in new_pnames:
        pn_out += nm.ljust(8, b"\0")[:8]

    # serialize new TEXTURE1
    headers = bytearray(struct.pack("<i", len(new_textures)))
    body = bytearray()
    base = 4 + 4 * len(new_textures)
    tex_offsets = []
    for (name, masked, w, h, coldir, patches) in new_textures:
        tex_offsets.append(base + len(body))
        body += name.ljust(8, b"\0")[:8]
        body += struct.pack("<ihhih", masked, w, h, coldir, len(patches))
        for ox, oy, pi, sd_, cm in patches:
            body += struct.pack("<hhhhh", ox, oy, remap[pi], sd_, cm)
    for to in tex_offsets:
        headers += struct.pack("<i", to)
    tex_out = bytes(headers) + bytes(body)

    keep_patch_names = set(new_pnames)

    # --- build output lump list ---
    out = []
    in_spr = in_pat = False
    i = 0
    while i < len(lumps):
        name, data = lumps[i]
        # maps: keep only E1M1
        if MAP_MARKER.match(name):
            grp = [(name, data)]
            j = i + 1
            while j < len(lumps) and lumps[j][0] in MAP_SUB:
                grp.append(lumps[j]); j += 1
            if name == KEEP_MAP:
                out.extend(grp)
            i = j
            continue
        # sprite namespace
        if name in (b"S_START", b"SS_START"): in_spr = True; out.append((name, data)); i += 1; continue
        if name in (b"S_END", b"SS_END"):     in_spr = False; out.append((name, data)); i += 1; continue
        # patch namespace
        if name in (b"P_START", b"P1_START", b"P2_START", b"P3_START", b"PP_START"):
            in_pat = True; out.append((name, data)); i += 1; continue
        if name in (b"P_END", b"P1_END", b"P2_END", b"P3_END", b"PP_END"):
            in_pat = False; out.append((name, data)); i += 1; continue

        drop = False
        if in_spr:
            drop = name[:4] not in KEEP_SPR
        elif in_pat:
            drop = name not in keep_patch_names
        else:
            if name in DROP_GFX: drop = True
            if name.startswith(b"DS") or name.startswith(b"DP"): drop = True   # sfx
            if name.startswith(b"D_"): drop = True                              # music
            if name.startswith(b"DEMO"): drop = True
            if name.startswith(b"WI"): drop = True   # intermission screens (no audio/short test)
            if name.startswith(b"M_"): drop = True   # menu graphics (autostart, no menu)
            if name == b"TITLEPIC": drop = True       # no title screen (autostart)
            if name == b"PNAMES": data = pn_out
            if name == b"TEXTURE1": data = tex_out
        if not drop:
            out.append((name, data))
        i += 1

    total = write_wad(dst, magic, out)
    print(f"E1M1 textures used: {len(used_tex)}, kept patches: {len(new_pnames)} / {npn}")
    print(f"out: {total} bytes ({total/1024:.0f} KB), {len(out)} lumps -> {dst}")

if __name__ == "__main__":
    main()
