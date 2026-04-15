#!/usr/bin/env python3
"""
Analyze CODE_24 music data in a DOS SR.EXE (already unpacked).
Finds distinct musical segments and the repeating cycle in each playlist block.

Usage:
    python3 analyze_music.py SR.EXE
"""

import sys
import struct
import hashlib
from pathlib import Path
from collections import defaultdict

GHIDRA_BASE   = 0x10000
CODE24_LINEAR = 0x3E6B0
CODE24_SIZE   = 4736
CODE24_SKIP   = 4
ISR_RATE_HZ   = 1193182 / 16384  # ~72.82 Hz

MUSIC_SIG = bytes([
    0x05, 0x00, 0x74, 0x04,
    0x05, 0x00, 0x4B, 0x05,
    0x05, 0x00, 0xF1, 0x05,
])
TEARDOWN_SIG = bytes([
    0xFA, 0xB8, 0x36, 0x00, 0xE6, 0x43, 0x2B, 0xC0, 0xE6, 0x40, 0xE6, 0x40
])


def load_exe(path):
    data = Path(path).read_bytes()
    if len(data) < 0x20 or data[0:2] != b'MZ':
        raise ValueError("not an MZ executable")
    load_base = struct.unpack_from('<H', data, 8)[0] * 16
    return data, load_base


def find_c24_offset(data, load_base):
    td_fo = (0x10fb7 - GHIDRA_BASE) + load_base
    if data[td_fo:td_fo + len(TEARDOWN_SIG)] != TEARDOWN_SIG:
        raise ValueError("TEARDOWN_SIG mismatch -- not SR.EXE")
    c24_fo = (CODE24_LINEAR - GHIDRA_BASE) + load_base
    if data[c24_fo + CODE24_SKIP:c24_fo + CODE24_SKIP + len(MUSIC_SIG)] != MUSIC_SIG:
        raise ValueError("MUSIC_SIG mismatch in CODE_24")
    return c24_fo


def parse_segments(data, c24_fo):
    raw = data[c24_fo:c24_fo + CODE24_SIZE]
    segments = []
    pos = CODE24_SKIP
    while pos + 4 <= CODE24_SIZE:
        notes = []
        while pos + 4 <= CODE24_SIZE:
            dur, pit = struct.unpack_from('<HH', raw, pos)
            pos += 4
            if dur == 0:
                break
            notes.append((dur, pit))
        if notes:
            segments.append(notes)
    return segments


def dur_sec(notes):
    return sum(d for d, _ in notes) / ISR_RATE_HZ


def full_hash(notes):
    h = hashlib.md5()
    for d, p in notes:
        h.update(struct.pack('<HH', d, p))
    return h.hexdigest()[:8]


def pitch_hash(notes):
    """Hash pitch sequence only, ignoring note durations."""
    h = hashlib.md5()
    for _, p in notes:
        h.update(struct.pack('<H', p))
    return h.hexdigest()[:8]


def is_bridge(notes):
    return bool(notes) and notes[0][0] == 5 and notes[0][1] == 0x0474


def is_rest(notes):
    return bool(notes) and all(p == 0 for _, p in notes)


def classify(notes):
    if is_bridge(notes): return 'BRIDGE'
    if is_rest(notes):   return 'REST'
    return 'SONG'


def find_cycle(seq):
    """Shortest prefix that tiles seq (ignoring 0x79AA wildcards)."""
    n = len(seq)
    for length in range(1, n // 2 + 1):
        pat = seq[:length]
        ok = True
        for i in range(n):
            a, b = seq[i], pat[i % length]
            # 0x79AA is a runtime random -- treat as wildcard
            if a == 0x79AA or b == 0x79AA:
                continue
            if a != b:
                ok = False
                break
        if ok:
            return pat
    return seq  # no shorter cycle found


def parse_playlist(data, c24_fo):
    pl_fo = c24_fo + 0x15B08  # DS:0x5858 -> file offset
    blocks, cur = [], []
    for i in range(512):
        off = pl_fo + i * 2
        if off + 2 > len(data):
            break
        v = struct.unpack_from('<H', data, off)[0]
        if v == 0xFFFF:
            if not cur:
                break          # empty block = end of playlist
            blocks.append(cur)
            cur = []
        elif v <= 4 or v == 0x79AA:
            cur.append(v)
        else:
            if cur:
                blocks.append(cur)
            break
    if cur:
        blocks.append(cur)
    return blocks


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} SR.EXE")
        sys.exit(1)

    data, load_base = load_exe(sys.argv[1])
    c24_fo = find_c24_offset(data, load_base)
    segs   = parse_segments(data, c24_fo)

    # Build pattern map: pat[0]=first bridge, pat[1..4]=song segs in order
    bridge_idx = next((i for i, s in enumerate(segs) if is_bridge(s)), None)
    song_idxs  = [i for i, s in enumerate(segs) if classify(s) == 'SONG']
    pat = {0: bridge_idx}
    for k, si in enumerate(song_idxs[:4]):
        pat[k + 1] = si

    fhash = [full_hash(s) for s in segs]
    phash = [pitch_hash(s) for s in segs]
    hash_groups = defaultdict(list)
    for i, h in enumerate(fhash):
        hash_groups[h].append(i)

    # --- Segment table ---
    print(f"CODE_24 segments: {len(segs)}\n")
    print(f"  {'i':>3}  {'type':>6}  {'notes':>5}  {'dur':>7}  {'full_hash'}  {'pitch_hash'}  pat  duplicates")
    for i, notes in enumerate(segs):
        kind  = classify(notes)
        dupes = [x for x in hash_groups[fhash[i]] if x != i]
        pat_k = next((k for k, v in pat.items() if v == i), '-')
        dup_s = f"  == seg{dupes}" if dupes else ""
        print(f"  {i:3d}  {kind:>6}  {len(notes):>5}  {dur_sec(notes):>7.2f}s"
              f"  {fhash[i]}  {phash[i]}  {str(pat_k):>3}{dup_s}")

    print()

    # --- Playlist blocks ---
    blocks = parse_playlist(data, c24_fo)
    print(f"Playlist blocks: {len(blocks)}\n")

    def resolve(e):
        if e == 0x79AA: return 'RND'
        si = pat.get(e)
        return f'p{e}=seg{si}' if si is not None else f'p{e}=?'

    for bi, block in enumerate(blocks):
        has_melody = any(e != 0 for e in block)
        btype = 'SONG' if has_melody else 'BRIDGE'

        cycle = find_cycle(block)
        repeats = len(block) // len(cycle)
        leftover = len(block) % len(cycle)

        # Duration of one cycle
        cyc_dur = 0.0
        for e in cycle:
            si = pat.get(e if e != 0x79AA else 1)
            if si is not None:
                cyc_dur += dur_sec(segs[si])

        total_dur = cyc_dur * repeats + (cyc_dur * leftover / len(cycle) if cycle else 0)

        cyc_str = ' '.join(resolve(e) for e in cycle)
        print(f"  block[{bi}] {btype}  {len(block)} plays"
              f"  cycle={cyc_dur:.1f}s x{repeats}"
              + (f"+{leftover}" if leftover else "")
              + f"  total={total_dur:.1f}s")
        print(f"    cycle: [{cyc_str}]")
        if len(block) != len(cycle):
            full_str = ' '.join(resolve(e) for e in block)
            print(f"    full:  [{full_str}]")
        print()

    # --- Distinct songs ---
    print("Distinct SONG segments (candidates for shuffle):")
    seen = set()
    for i in song_idxs:
        h = fhash[i]
        if h not in seen:
            seen.add(h)
            ph = phash[i]
            dupes = [x for x in hash_groups[h] if x != i]
            dup_s = f"  (also seg{dupes})" if dupes else ""
            print(f"  seg[{i:2d}]  {dur_sec(segs[i]):.2f}s  {h}  pitch={ph}{dup_s}")


if __name__ == '__main__':
    main()
