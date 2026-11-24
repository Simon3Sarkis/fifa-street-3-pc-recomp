"""Contact sheet of a run's in-game snaps: snapsheet.py <run> [<run> ...] -> work/snaps/<first>_snaps.png
One row per run, frames in name order (settings first)."""
import glob, os, sys
from PIL import Image, ImageDraw
W, H = 480, 270
rows = []
for run in sys.argv[1:]:
    files = sorted(glob.glob(f'work/snaps/{run}/*.png'), key=lambda f: (not os.path.basename(f).startswith('settings'), f))
    rows.append((run, files))
cols = max(len(f) for _, f in rows)
sheet = Image.new('RGB', (cols * W, len(rows) * H))
for r, (run, files) in enumerate(rows):
    for c, f in enumerate(files):
        im = Image.open(f).convert('RGB').resize((W, H)); d = ImageDraw.Draw(im)
        label = f'{run} {os.path.basename(f)[:-4]}'; d.rectangle((0, 0, 7 * len(label) + 6, 14), fill=(0, 0, 0)); d.text((3, 2), label, fill=(255, 255, 0))
        sheet.paste(im, (c * W, r * H))
out = f'work/snaps/{sys.argv[1]}_snaps.png'; sheet.save(out); print(out)
