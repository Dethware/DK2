"""Readers for DK2 *BANK.map and *SFX.map files.

Layouts follow OpenKeeper's BankMapFile / SfxMapFile (tonihele/OpenKeeper),
field names here are our own guesses where OpenKeeper has "unknown".

BANK.map:
  guid[16], u32 unk[2], u32 count
  count x { u32 -1, u32 junk, u8 junk[3] }       (11 bytes, runtime garbage)
  count x { u32 len, char name[len] }             archive names, e.g. Global\\Track_1_1

SFX.map:
  guid[16], u32 unk[2], u32 count
  count x Entry  (24): u32 n_groups, u32 unk, u16 unk, u16 unk, f32 min_dist, f32 max_dist, f32 scale
  sum(n_groups) x Group (20): u32 type_id, u32 n_items, u32 unk[3]
  per group:
    n_items x Item (42): u32 n_sounds, u32 n_links, u32 junk, u8 params[26], u32 junk
    per item: n_sounds x Sound (16): u32 index, u32 unk1, u32 unk2, u32 archive
              n_links  x Link  (8):  u32 target, u8 b0, u8 b1, u8 lo, u8 hi
"""
import struct
import sys


def read_bank_map(path):
    d = open(path, "rb").read()
    (count,) = struct.unpack_from("<I", d, 0x18)
    o = 0x1C + count * 11
    names = []
    for _ in range(count):
        (n,) = struct.unpack_from("<I", d, o)
        names.append(d[o + 4:o + 4 + n].split(b"\0", 1)[0].decode("latin-1"))
        o += 4 + n
    return names


def read_sfx_map(path):
    d = open(path, "rb").read()
    (count,) = struct.unpack_from("<I", d, 0x18)
    o = 0x1C
    entries = []
    for _ in range(count):
        ng, u1, u2, u3, mn, mx, sc = struct.unpack_from("<IIHHfff", d, o)
        entries.append(dict(n_groups=ng, unk=(u1, u2, u3), min_dist=mn, max_dist=mx, scale=sc, groups=[]))
        o += 24
    for e in entries:
        for _ in range(e["n_groups"]):
            tid, n, a, b, c = struct.unpack_from("<5I", d, o)
            e["groups"].append(dict(type_id=tid, n_items=n, unk=(a, b, c), items=[]))
            o += 20
    for e in entries:
        for g in e["groups"]:
            for _ in range(g["n_items"]):
                ns, nl = struct.unpack_from("<II", d, o)
                params = d[o + 12:o + 38]
                g["items"].append(dict(n_sounds=ns, n_links=nl, params=params, sounds=[], links=[]))
                o += 42
            for it in g["items"]:
                for _ in range(it["n_sounds"]):
                    it["sounds"].append(struct.unpack_from("<4I", d, o))
                    o += 16
                for _ in range(it["n_links"]):
                    it["links"].append(struct.unpack_from("<I4B", d, o))
                    o += 8
    assert o == len(d), (o, len(d))
    return entries


def main():
    bank_path, sfx_path = sys.argv[1], sys.argv[2]
    sdt_names = None
    if len(sys.argv) > 3:
        sys.path.insert(0, sys.argv[3])
    banks = read_bank_map(bank_path)
    print("banks:", ", ".join(f"{i + 1}={b}" for i, b in enumerate(banks)))
    for ei, e in enumerate(read_sfx_map(sfx_path)):
        print(f"entry {ei}: unk={e['unk']} min={e['min_dist']} max={e['max_dist']} scale={e['scale']}")
        for g in e["groups"]:
            print(f"  group type_id={g['type_id']} items={g['n_items']} unk={g['unk']}")
            for ii, it in enumerate(g["items"]):
                snd = " ".join(f"[idx={s[0]} u1={s[1]:x} u2={s[2]:x} bank={s[3]}]" for s in it["sounds"])
                lnk = " ".join(f"{t}@{lo}-{hi}" + (f"({b0},{b1})" if b0 or b1 else "") for t, b0, b1, lo, hi in it["links"])
                print(f"    {ii + 1:3d} params={it['params'].hex()} {snd} -> {lnk}")


if __name__ == "__main__":
    main()
