"""Write an input script that walks the menus and snaps each step: explore.py <out.txt> <steps...>

From a saved profile (run with -Save work\\saves\\fresh_profile): title -> main menu, then each step.
Steps:
  A B X Y START BACK UP DOWN LEFT RIGHT LB RB   press it (a snap is taken just before)
  DOWN*3                                        press it 3 times, 600 ms apart (no snaps between)
  w5000                                         wait 5000 ms before the next step (default 3500)
  stick:x:y:ms                                  left stick for ms
  hold:A:ms                                     hold a button
  snap                                          an extra snap now
  u1 / u0                                       following presses go to controller 2 / 1
  quit                                          (added at the end anyway)
Snaps are named s00, s01, ... so a contact sheet (tools/snapsheet.py) shows the walk in order.
"""
import sys

out, steps = sys.argv[1], sys.argv[2:]
lines = [
    '0      log boot',
    '0      waitpixel 410 446 231 231 231 24 120000',
    '+1500  pressuntil START 1500 296 104 230 230 230 24 30000',
    '+2500  snap s00',
]
n = 1
wait = 3500
for step in steps:
    if step.startswith('w') and step[1:].isdigit():
        wait = int(step[1:])
        continue
    if step == 'snap':
        lines.append(f'+{wait}  snap s{n:02d}'); n += 1
        continue
    if step in ('u0', 'u1', 'u2', 'u3'):
        lines.append(f'+0     user {step[1]}')  # following presses go to this controller
        continue
    if step == 'quit':
        break
    if step.startswith('stick:'):
        _, x, y, ms = step.split(':')
        lines.append(f'+{wait}  snap s{n:02d}'); n += 1
        lines.append(f'+200   stick L {x} {y} {ms}')
        continue
    if step.startswith('hold:'):
        _, b, ms = step.split(':')
        lines.append(f'+{wait}  snap s{n:02d}'); n += 1
        lines.append(f'+200   hold {b} {ms}')
        continue
    if '*' in step:
        b, k = step.split('*')
        lines.append(f'+{wait}  snap s{n:02d}'); n += 1
        for i in range(int(k)):
            lines.append(f'+{200 if i == 0 else 600}   press {b} 120')
        continue
    lines.append(f'+{wait}  snap s{n:02d}'); n += 1
    lines.append(f'+200   press {step} 150')
lines.append(f'+{wait}  snap s{n:02d}')
lines.append('+500   quit')
open(out, 'w').write('\n'.join(lines) + '\n')
print(out, len(lines), 'lines,', n + 1, 'snaps')
