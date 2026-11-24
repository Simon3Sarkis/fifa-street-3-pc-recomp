"""Check a recording of the game's audio output for problems: audio_check.py <file.wav> [more.wav ...]

Reports per file: format and length; peak / RMS level and the per-second level range; dropouts (runs
of >= 10 ms of near-silence with audible sound on both sides - what an underrun sounds like, as
opposed to the game going quiet); clicks (second-difference outliers far above the local level);
clipping; DC offset, left/right balance and correlation; spectral roll-off. Writes <file>.png with the
envelope and a spectrogram. Needs numpy, scipy, matplotlib.
"""
import sys

import numpy as np
from scipy.io import wavfile
from scipy import signal

MIN_GAP_S = 0.004  # one 256-sample guest audio frame is 5.3 ms


def db(x):
    return 20 * np.log10(np.maximum(x, 1e-9))


def check(path):
    rate, data = wavfile.read(path)
    if data.dtype == np.int16:
        x = data.astype(np.float64) / 32768.0
    else:
        x = data.astype(np.float64)
    if x.ndim == 1:
        x = x[:, None]
    n, ch = x.shape
    dur = n / rate
    mono = x.mean(axis=1)
    print(f'{path}: {rate} Hz, {ch} ch, {dur:.1f} s')
    peak = np.abs(x).max()
    rms = np.sqrt((x ** 2).mean())
    sec = rate
    per_sec = [db(np.sqrt((mono[i:i + sec] ** 2).mean())) for i in range(0, n - sec + 1, sec)]
    print(f'  level: peak {db(peak):.1f} dBFS, RMS {db(rms):.1f} dBFS, per-second RMS '
          f'{min(per_sec):.1f} .. {max(per_sec):.1f} dBFS')

    # Dropouts: near-silent runs (|x| < 2 LSB on all channels) of >= MIN_GAP_S (4 ms), with the 200 ms before and
    # after both audible (RMS > -45 dBFS).
    quiet = (np.abs(x) < 2 / 32768).all(axis=1)
    edges = np.diff(np.concatenate([[0], quiet.astype(np.int8), [0]]))
    starts, ends = np.where(edges == 1)[0], np.where(edges == -1)[0]
    win = int(0.2 * rate)
    drops = []
    for s, e in zip(starts, ends):
        if (e - s) < MIN_GAP_S * rate or s < win or e + win > n:
            continue
        before = db(np.sqrt((mono[s - win:s] ** 2).mean()))
        after = db(np.sqrt((mono[e:e + win] ** 2).mean()))
        if before > -45 and after > -45:
            drops.append((s / rate, (e - s) / rate * 1000))
    print(f'  dropouts (>=4 ms silence inside audible sound): {len(drops)}'
          + ('' if not drops else ' at ' + ', '.join(f'{t:.2f}s ({ms:.0f} ms)' for t, ms in drops[:8])))

    # Clicks: |second difference| far above a robust local scale (median absolute deviation over
    # 50 ms windows), and large in absolute terms.
    d2 = np.abs(np.diff(mono, 2))
    w = int(0.05 * rate)
    clicks = []
    for i in range(0, len(d2) - w, w):
        seg = d2[i:i + w]
        scale = np.median(seg) + 1e-6
        j = int(seg.argmax())
        if seg[j] > 0.2 and seg[j] > 40 * scale:
            t = (i + j) / rate
            if not clicks or t - clicks[-1] > 0.010:  # one click straddling two windows counts once
                clicks.append(t)
    print(f'  click candidates: {len(clicks)} ({len(clicks) / dur * 60:.1f}/min)'
          + ('' if not clicks else ' e.g. at ' + ', '.join(f'{t:.2f}s' for t in clicks[:8])))

    clip = int((np.abs(x) >= 32766 / 32768).sum())
    print(f'  clipped samples: {clip}')
    if ch >= 2:
        l, r = x[:, 0], x[:, 1]
        bal = db(np.sqrt((l ** 2).mean())) - db(np.sqrt((r ** 2).mean()))
        corr = np.corrcoef(l, r)[0, 1] if l.std() > 0 and r.std() > 0 else float('nan')
        print(f'  DC offset L {l.mean() * 1000:.2f} / R {r.mean() * 1000:.2f} (x1e-3 FS), '
              f'L-R balance {bal:+.1f} dB, L/R correlation {corr:.2f}')
    f, p = signal.welch(mono, rate, nperseg=8192)
    cum = np.cumsum(p) / p.sum()
    roll95 = f[np.searchsorted(cum, 0.95)]
    hi = p[f > 12000].sum() / p.sum() * 100
    print(f'  spectrum: 95% of energy below {roll95:.0f} Hz, {hi:.2f}% above 12 kHz')

    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    fig, (a1, a2) = plt.subplots(2, 1, figsize=(12, 6), sharex=True)
    hop = rate // 100
    env = [np.abs(mono[i:i + hop]).max() for i in range(0, n - hop, hop)]
    a1.plot(np.arange(len(env)) / 100, env, lw=0.6)
    for t, _ in drops:
        a1.axvline(t, color='r', lw=0.8)
    for t in clicks:
        a1.axvline(t, color='orange', lw=0.5, alpha=0.6)
    a1.set_ylabel('peak |x| per 10 ms')
    a1.set_title(f'{path}  (red: dropouts, orange: click candidates)')
    ff, tt, ss = signal.spectrogram(mono, rate, nperseg=2048, noverlap=1024)
    a2.pcolormesh(tt, ff, 10 * np.log10(ss + 1e-12), shading='auto', vmin=-120, vmax=-30)
    a2.set_ylabel('Hz')
    a2.set_xlabel('s')
    fig.tight_layout()
    fig.savefig(path + '.png', dpi=90)
    plt.close(fig)
    return {'drops': len(drops), 'clicks': len(clicks), 'clip': clip}


if __name__ == '__main__':
    for p in sys.argv[1:]:
        check(p)
