"""Pixel diff of snaps from one run: imgdiff.py <run> <ref> <other>...

Compares work/snaps/<run>/<other>.png against <ref>.png and prints the number of differing pixels,
the largest channel difference and the bounding box of the changes. Writes <other>_diff.png
(differences amplified) when anything differs.
"""
import sys

from PIL import Image, ImageChops

run, ref, others = sys.argv[1], sys.argv[2], sys.argv[3:]
a = Image.open(f'work/snaps/{run}/{ref}.png').convert('RGB')
for name in others:
    b = Image.open(f'work/snaps/{run}/{name}.png').convert('RGB')
    d = ImageChops.difference(a, b)
    bbox = d.getbbox()
    px = d.get_flattened_data() if hasattr(d, 'get_flattened_data') else d.getdata()
    n = sum(1 for p in px if p != (0, 0, 0))
    mx = max(d.getextrema(), key=lambda e: e[1])[1]
    print(f'{ref} vs {name}: {n} px differ, max channel diff {mx}, bbox {bbox}')
    if n:
        d.point(lambda v: min(255, v * 16)).save(f'work/snaps/{run}/{name}_diff.png')
