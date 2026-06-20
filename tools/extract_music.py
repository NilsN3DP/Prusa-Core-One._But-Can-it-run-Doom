#!/usr/bin/env python3
"""Extract a (up to) 4-voice arrangement from a Doom MUS lump (e.g. D_E1M1 =
"At Doom's Gate") for the stepper-music player.

The MUS channels are sampled on a common timeline; the four most prominent
non-percussion channels become four voices, ordered by pitch and mapped to the
motors:  X = highest (melody), Y = 2nd, E = mid, Z = lowest (bass).

Outputs song_x/y/z/e[] (Hz, 0 = rest), song_d[] (ms) and SONG_LEN.

Usage: extract_music.py <wad> <lumpname> <out.h>
"""
import sys, struct

def find_lump(data, want):
    _, n, info = struct.unpack_from("<4sii", data, 0)
    for i in range(n):
        pos, size, raw = struct.unpack_from("<ii8s", data, info + i * 16)
        if raw.split(b"\0")[0] == want.encode():
            return data[pos:pos + size]
    raise SystemExit("lump %s not found" % want)

def parse_mus(mus):
    """Return slices: list of (dict channel->highest active note, duration ticks)."""
    assert mus[:4] == b"MUS\x1a", "not a MUS lump"
    p = struct.unpack_from("<H", mus, 6)[0]   # score start
    active = {}                                # channel -> set(notes)
    slices = []
    while p < len(mus):
        ev = mus[p]; p += 1
        last = ev >> 7; typ = (ev >> 4) & 7; ch = ev & 15
        if typ == 0:                           # release note
            active.setdefault(ch, set()).discard(mus[p] & 0x7f); p += 1
        elif typ == 1:                         # play note
            nb = mus[p]; p += 1
            active.setdefault(ch, set()).add(nb & 0x7f)
            if nb & 0x80:
                p += 1
        elif typ == 2:                         # pitch wheel
            p += 1
        elif typ == 3:                         # system event
            p += 1
        elif typ == 4:                         # controller
            p += 2
        elif typ == 6:                         # score end
            break
        if last:
            delay = 0
            while True:
                b = mus[p]; p += 1
                delay = delay * 128 + (b & 0x7f)
                if not (b & 0x80):
                    break
            snap = {c: max(ns) for c, ns in active.items() if c != 15 and ns}
            slices.append((snap, delay))
    return slices

def midi_to_hz(note):
    return 0 if note is None else round(440 * 2 ** ((note - 69) / 12))

def main():
    wad, lump, out = sys.argv[1], sys.argv[2], sys.argv[3]
    slices = parse_mus(find_lump(open(wad, "rb").read(), lump))

    # rank non-percussion channels by total sounding time, pick up to 4
    dur = {}; pitchsum = {}
    for snap, d in slices:
        for c, note in snap.items():
            dur[c] = dur.get(c, 0) + d
            pitchsum[c] = pitchsum.get(c, 0) + note * d
    chans = sorted(dur, key=lambda c: dur[c], reverse=True)[:4]
    chans.sort(key=lambda c: pitchsum[c] / dur[c], reverse=True)  # high -> low pitch
    # map by pitch: X=highest, Y=2nd, E=mid, Z=lowest
    order = [None, None, None, None]            # [X, Y, Z, E]
    names = ["X", "Y", "Z", "E"]
    if len(chans) == 1:
        order = [chans[0]] * 4
    elif len(chans) == 2:
        order = [chans[0], chans[0], chans[1], chans[1]]
    elif len(chans) == 3:
        order = [chans[0], chans[1], chans[2], chans[0]]   # E doubles the lead
    else:
        order = [chans[0], chans[1], chans[3], chans[2]]   # X,Y,Z(bass),E(mid)
    print("channels used:", chans,
          "-> X=%s Y=%s Z=%s E=%s" % tuple(order))

    # build per-voice frequency rows + durations (ms), merging tiny slices
    rows = []
    for snap, d in slices:
        ms = round(d * 1000 / 140)
        freqs = [midi_to_hz(snap.get(ch)) for ch in order]
        if ms < 15 and rows:                  # fold ultra-short slice into previous
            rows[-1][1] += ms
        else:
            rows.append([freqs, ms])
    # merge consecutive identical rows
    merged = []
    for freqs, ms in rows:
        if merged and merged[-1][0] == freqs:
            merged[-1][1] += ms
        else:
            merged.append([freqs, ms])

    cols = list(zip(*[f for f, _ in merged]))   # 4 columns of frequencies
    print("slices:", len(merged))

    with open(out, "w") as h:
        h.write("/* Generated from %s by tools/extract_music.py - NOT committed (game data). */\n" % lump)
        for i, nm in enumerate(names):
            h.write("static const uint16_t song_%s[] = {" % nm.lower())
            h.write(",".join(str(f) for f in cols[i]))
            h.write("};\n")
        h.write("static const uint16_t song_d[] = {")
        h.write(",".join(str(ms) for _, ms in merged))
        h.write("};\n")
        h.write("#define SONG_LEN (sizeof(song_d)/sizeof(song_d[0]))\n")
    print("wrote", out)

if __name__ == "__main__":
    main()
