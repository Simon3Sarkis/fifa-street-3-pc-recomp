"""Contact sheet of run screenshots: sheet.py <prefix> [columns]  -> work/snaps/<prefix>_sheet.png"""
import glob, re, sys
from PIL import Image, ImageDraw
prefix = sys.argv[1]; cols = int(sys.argv[2]) if len(sys.argv) > 2 else 4
files = sorted(glob.glob(f'work/snaps/{prefix}_*s.png'), key=lambda f: int(re.search(r'_(\d+)s\.png$', f)[1]))
w, h = 480, 270
rows = (len(files) + cols - 1) // cols
sheet = Image.new('RGB', (cols * w, rows * h))
for i, f in enumerate(files):
    im = Image.open(f).convert('RGB').resize((w, h))
    d = ImageDraw.Draw(im); t = re.search(r'_(\d+)s\.png$', f)[1] + 's'
    d.rectangle((0, 0, 44, 16), fill=(0, 0, 0)); d.text((4, 2), t, fill=(255, 255, 0))
    sheet.paste(im, ((i % cols) * w, (i // cols) * h))
out = f'work/snaps/{prefix}_sheet.png'; sheet.save(out); print(out, len(files), 'frames')
