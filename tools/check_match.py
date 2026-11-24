"""Did a run reach a match? check_match.py <run> [snap names...]

Looks for the in-match scoreboard in work/snaps/<run>/<name>.png: the Brazil flag (green, top-left)
and the score bar outline next to it, at the positions the Samba match HUD uses at any resolution.
Prints one line per snap and exits 1 if none of them shows a match, so a perf run that fell back to
the title screen can't be mistaken for match numbers (2026-11-23: four "baselines" were title screens).
"""
import glob
import os
import sys

from PIL import Image

run = sys.argv[1]
names = sys.argv[2:] or [os.path.basename(f)[:-4] for f in sorted(glob.glob(f'work/snaps/{run}/match_*.png'))]
hits = 0
for name in names:
    path = f'work/snaps/{run}/{name}.png'
    if not os.path.exists(path):
        print(f'{name}: missing')
        continue
    im = Image.open(path).convert('RGB').resize((1280, 720))
    # Brazil flag map on the HUD: mostly green around (270, 85) in 1280x720.
    flag = im.crop((240, 65, 300, 110))
    px = list(flag.get_flattened_data() if hasattr(flag, "get_flattened_data") else flag.getdata())
    green = sum(1 for r, g, b in px if g > 110 and g > r * 1.3 and g > b * 1.6) / len(px)
    # Title screen: large beige/tan background around the centre-left.
    title = im.crop((60, 400, 300, 600))
    tp = list(title.get_flattened_data() if hasattr(title, "get_flattened_data") else title.getdata())
    beige = sum(1 for r, g, b in tp if r > 150 and g > 120 and b > 80 and r > b + 30) / len(tp)
    is_match = green > 0.25 and beige < 0.5
    hits += is_match
    print(f'{name}: {"MATCH" if is_match else "not a match"} (flag green {green:.0%}, beige {beige:.0%})')
print(f'{run}: {hits}/{len(names)} snaps in a match')
sys.exit(0 if hits else 1)
