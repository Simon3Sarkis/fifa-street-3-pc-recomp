"""Extract one front-end screen of FIFA Street 3 for study (read only; never write game files back).

fe_extract.py <game/data/fe/fe.big> <out dir> data/fe/69_gameSettings.big [...]

fe.big is a BIG4 archive of per-screen BIGF archives, each RefPack-compressed (0x10FB). A screen
holds "Apt Data" (the movie: display list and ActionScript bytecode), "Apt const" (its constant
pool: identifiers and strings, readable with any strings tool) and geometry / texture files.
Write the output to a scratch folder, not into the repo: it is game data.
"""
import os
import struct
import sys


def refpack(src):
    flags = src[0] << 8 | src[1]
    p = 2
    big = bool(flags & 0x8000)
    if flags & 0x0100:  # compressed size present
        p += 4 if big else 3
    n = 4 if big else 3
    size = int.from_bytes(src[p:p + n], 'big')
    p += n
    out = bytearray()
    while p < len(src):
        b0 = src[p]
        if b0 < 0x80:
            b1 = src[p + 1]
            p += 2
            plain, cnt, off = b0 & 3, ((b0 & 0x1C) >> 2) + 3, ((b0 & 0x60) << 3) + b1 + 1
        elif b0 < 0xC0:
            b1, b2 = src[p + 1], src[p + 2]
            p += 3
            plain, cnt, off = b1 >> 6, (b0 & 0x3F) + 4, ((b1 & 0x3F) << 8) + b2 + 1
        elif b0 < 0xE0:
            b1, b2, b3 = src[p + 1], src[p + 2], src[p + 3]
            p += 4
            plain = b0 & 3
            cnt = ((b0 & 0x0C) << 6) + b3 + 5
            off = ((b0 & 0x10) << 12) + (b1 << 8) + b2 + 1
        elif b0 < 0xFC:
            p += 1
            plain, cnt, off = ((b0 & 0x1F) << 2) + 4, 0, 0
        else:
            p += 1
            plain, cnt, off = b0 & 3, 0, 0
        out += src[p:p + plain]
        p += plain
        for _ in range(cnt):
            out.append(out[-off])
        if b0 >= 0xFC:
            break
    return bytes(out[:size])


def read_big(data):
    n = struct.unpack('>I', data[8:12])[0]
    p = 16
    out = []
    for _ in range(n):
        off, size = struct.unpack('>II', data[p:p + 8])
        p += 8
        e = data.index(b'\0', p)
        out.append((data[p:e].decode('latin1'), off, size))
        p = e + 1
    return out


def main():
    data = open(sys.argv[1], 'rb').read()
    out_dir = sys.argv[2]
    top = {n: (o, s) for n, o, s in read_big(data)}
    for want in sys.argv[3:]:
        o, s = top[want]
        sub = data[o:o + s]
        if sub[:2] == b'\x10\xfb':
            sub = refpack(sub)
        d = os.path.join(out_dir, os.path.basename(want))
        os.makedirs(d, exist_ok=True)
        for name, fo, fs in read_big(sub):
            with open(os.path.join(d, name.replace('/', '_').replace('\\', '_')), 'wb') as f:
                f.write(sub[fo:fo + fs])
            print(want, name, fs, sub[fo:fo + 12])


if __name__ == '__main__':
    main()
