"""Untile Xbox 360 (Xenos) 2D-tiled 32bpp surfaces dumped by DebugDumpResolveReadback.

    untile.py <out.png> <width> <height> <dump.bin> [<dump.bin> ...]

Each dump becomes a tile in a sheet: RGB on the left, alpha (as grey) on the right. Bytes are
taken as big-endian 8888 (A,R,G,B order after the 8-in-32 swap is guessed by trying both and
using the one with more variance in the RGB channels is NOT done - we show raw byte order 0..3
as R,G,B,A and say so). Tiling formula: Xenia's texture_util GetTiledOffset2D.
"""
import os
import sys

from PIL import Image, ImageDraw


def tiled_offset_2d(x, y, pitch, bpb_log2):
    pitch = (pitch + 31) & ~31
    macro = ((x >> 5) + (y >> 5) * (pitch >> 5)) << (bpb_log2 + 7)
    micro = ((x & 7) + ((y & 0xE) << 2)) << bpb_log2
    offset = macro + ((micro & ~0xF) << 1) + (micro & 0xF) + ((y & 1) << 4)
    return (((offset & ~0x1FF) << 3) + ((y & 16) << 7) + ((offset & 0x1C0) << 2) +
            (((((y & 8) >> 2) + (x >> 3)) & 3) << 6) + (offset & 0x3F))


def untile(data, w, h):
    rgb = Image.new('RGB', (w, h))
    alpha = Image.new('L', (w, h))
    pr, pa = rgb.load(), alpha.load()
    for y in range(h):
        for x in range(w):
            o = tiled_offset_2d(x, y, w, 2)
            if o + 4 > len(data):
                continue
            b0, b1, b2, b3 = data[o:o + 4]
            # Stored big-endian ARGB-ish: show bytes 1..3 as RGB and byte 0 as alpha.
            pr[x, y] = (b1, b2, b3)
            pa[x, y] = b0
    return rgb, alpha


out, w, h = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
files = sys.argv[4:]
cell = 2 * w + 8
sheet = Image.new('RGB', (cell, (h + 16) * len(files)), (40, 40, 40))
d = ImageDraw.Draw(sheet)
for i, f in enumerate(files):
    rgb, a = untile(open(f, 'rb').read(), w, h)
    y0 = i * (h + 16)
    d.text((2, y0 + 2), os.path.basename(f)[:60], fill=(255, 255, 0))
    sheet.paste(rgb, (0, y0 + 14))
    sheet.paste(a.convert('RGB'), (w + 8, y0 + 14))
sheet.save(out)
print(out)
