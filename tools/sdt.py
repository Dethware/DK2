"""Reader for DK2 .sdt sound archives.

Layout (little-endian):
  u32 count
  u32 offsets[count]           absolute offset of each entry
  entry:
    u32 header_size            always 0x28
    u32 data_size
    char name[16]              NUL-padded
    u16 sample_rate, u8 bits_per_sample, u8 type   (printed together as unk0)
    u32 0
    u32 pcm_size               decoded PCM byte size
    u32 0
    u8  data[data_size]        MPEG audio (MP2) or other payload

See docs/file-formats.md.
"""
import struct
import sys
from dataclasses import dataclass


@dataclass
class Entry:
    index: int
    name: str
    offset: int
    data_size: int
    unk0: int
    unk1: int
    pcm_size: int
    unk2: int
    data: bytes


def read_sdt(path):
    with open(path, "rb") as f:
        d = f.read()
    (count,) = struct.unpack_from("<I", d, 0)
    offsets = struct.unpack_from("<%dI" % count, d, 4)
    entries = []
    for i, o in enumerate(offsets):
        hsize, dsize = struct.unpack_from("<II", d, o)
        name = d[o + 8:o + 24].split(b"\0", 1)[0].decode("latin-1")
        unk0, unk1, pcm, unk2 = struct.unpack_from("<4I", d, o + 24)
        data = d[o + hsize:o + hsize + dsize]
        entries.append(Entry(i, name, o, dsize, unk0, unk1, pcm, unk2, data))
    return entries


def main():
    import os
    args = sys.argv[1:]
    out = None
    if args and args[0] == "-x":
        out = args[1]
        args = args[2:]
    for path in args:
        print(path)
        for e in read_sdt(path):
            print(f"  {e.index:3d} {e.name:16s} size={e.data_size:8d} pcm={e.pcm_size:9d} "
                  f"unk0={e.unk0:08x} unk1={e.unk1:x} unk2={e.unk2:x} magic={e.data[:4].hex()}")
            if out:
                base = os.path.splitext(os.path.basename(path))[0]
                dst = os.path.join(out, base)
                os.makedirs(dst, exist_ok=True)
                with open(os.path.join(dst, f"{e.index:03d}_{e.name}"), "wb") as f:
                    f.write(e.data)


if __name__ == "__main__":
    main()
