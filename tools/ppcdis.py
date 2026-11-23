"""Disassemble the dumped FIFA Street 3 image: ppcdis.py <addr> [count] [bin]

Marks .pdata function starts with '==>' so function boundaries are visible.
"""
import struct
import sys

import capstone

BASE = 0x82000000
PDATA, PDATA_COUNT = 0x820EAA00, 14375
BIN = sys.argv[3] if len(sys.argv) > 3 else 'work/xexdump/default_82000000_008A0000.bin'

data = open(BIN, 'rb').read()
starts = {}
for i in range(PDATA_COUNT):
    b, info = struct.unpack_from('>II', data, PDATA - BASE + i * 8)
    starts[b] = ((info >> 8) & 0x3FFFFF) * 4

addr = int(sys.argv[1], 16)
count = int(sys.argv[2]) if len(sys.argv) > 2 else 24
md = capstone.Cs(capstone.CS_ARCH_PPC, capstone.CS_MODE_32 | capstone.CS_MODE_BIG_ENDIAN)
for i in range(count):
    a = addr + 4 * i
    word = data[a - BASE:a - BASE + 4]
    mark = f'==> pdata size 0x{starts[a]:X}' if a in starts else ''
    ins = next(md.disasm(word, a), None)
    text = f'{ins.mnemonic} {ins.op_str}' if ins else '.long'
    print(f'{a:08X}  {word.hex().upper()}  {text:40} {mark}')
