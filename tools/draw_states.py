"""Decode [draw state] lines (debug_log_draws_ps) for one frame: draw_states.py <log> [frame]

Groups the frame's draws by (pixel shader, targets, depth/stencil, blend) in submission order and
decodes the Xenos render-backend registers that matter for effects like shadows: colour formats,
depth format, z/stencil enable and functions, stencil ops, colour write mask, blending.
"""
import re
import sys

LINE = re.compile(r'\[draw state\] frame=(\d+) vs=(\w+) ps=(\w+) prim=(\d+) idx=(\d+) \| (.*) norm_mask=(\w+)')
COLOR_FMT = {0: '8888', 1: '8888_GAMMA', 2: '2_10_10_10', 3: '2_10_10_10_FLOAT', 4: '16_16', 5: '16_16_16_16',
             6: '16_16_FLOAT', 7: '16_16_16_16_FLOAT', 10: '2_10_10_10_AS_10_10_10_10',
             12: '2_10_10_10_FLOAT_AS_16_16_16_16', 14: '32_FLOAT', 15: '32_32_FLOAT'}
DEPTH_FMT = {0: 'D24S8', 1: 'D24FS8'}
FUNC = ['never', 'less', 'equal', 'lequal', 'greater', 'notequal', 'gequal', 'always']
SOP = ['keep', 'zero', 'repl', 'incrsat', 'decrsat', 'invert', 'incr', 'decr']
MSAA = ['1x', '2x', '4x', '?']


def regs(text):
    out = {}
    for m in re.finditer(r'(\w+)=([0-9A-F,]+)', text):
        out[m[1]] = [int(v, 16) for v in m[2].split(',')]
    return out


def describe(r, norm_mask):
    s = r['SURFACE_INFO'][0]
    msaa = MSAA[(s >> 16) & 3]
    pitch = s & 0x3FFF
    mode = r['MODECONTROL'][0] & 7  # 4 = color+depth, 6 = copy, 5 = depth only? (edram mode)
    colors = []
    for i, ci in enumerate(r['COLOR_INFO']):
        if (norm_mask >> (4 * i)) & 0xF:
            colors.append(f'RT{i}:{COLOR_FMT.get((ci >> 16) & 0xF, (ci >> 16) & 0xF)}@{ci & 0xFFF}')
    dc = r['DEPTHCONTROL'][0]
    di = r['DEPTH_INFO'][0]
    parts = [f'{msaa} pitch{pitch} mode{mode}', ' '.join(colors) or 'no-color']
    z_enable, z_write = dc & 2, dc & 4
    if z_enable:
        parts.append(f'Z {FUNC[(dc >> 4) & 7]}{" W" if z_write else ""} {DEPTH_FMT.get((di >> 16) & 1)}@{di & 0xFFF}')
    if dc & 1:
        srm = r['STENCILREFMASK'][0]
        parts.append(f'STENCIL {FUNC[(dc >> 8) & 7]} ref{srm & 0xFF:02X} mask{(srm >> 8) & 0xFF:02X} '
                     f'wmask{(srm >> 16) & 0xFF:02X} fail:{SOP[(dc >> 11) & 7]} pass:{SOP[(dc >> 14) & 7]} '
                     f'zfail:{SOP[(dc >> 17) & 7]}{" 2SIDED" if dc & 0x80 else ""}')
    b = r['BLEND0'][0]
    colorcontrol = r['COLORCONTROL'][0]
    if colors:
        parts.append(f'blend0={b:08X} colorctl={colorcontrol:08X}')
    return ' | '.join(parts)


lines = [m for m in (LINE.search(l) for l in open(sys.argv[1], encoding='utf-8', errors='replace')) if m]
frames = sorted({int(m[1]) for m in lines})
frame = int(sys.argv[2]) if len(sys.argv) > 2 else (frames[1] if len(frames) > 1 else frames[0])
print(f'frames logged: {frames[:6]}; showing {frame}')
prev = None
run = 0
for m in [m for m in lines if int(m[1]) == frame] + [None]:
    key = None
    if m:
        r = regs(m[6])
        key = (m[3], describe(r, int(m[7], 16)))
    if key != prev and prev is not None:
        print(f'  x{run:<4} ps {prev[0]}  {prev[1]}')
        run = 0
    prev = key
    run += 1
