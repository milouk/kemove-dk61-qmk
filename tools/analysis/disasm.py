#!/usr/bin/env python3
"""Disassemble address ranges of a raw Thumb image, resolving literal-pool loads.

Usage: disasm.py IMAGE.bin BASE_HEX START-END [START-END ...]
"""
import sys, struct, re
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_MODE_MCLASS
fw = open(sys.argv[1], 'rb').read(); BASE = int(sys.argv[2], 16)
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB | CS_MODE_MCLASS); md.skipdata = True
for rng in sys.argv[3:]:
    a, b = [int(x, 16) for x in rng.split('-')]
    print('---- %05x-%05x' % (a, b))
    for i in md.disasm(fw[a - BASE:b - BASE], a):
        extra = ''
        m = re.search(r'\[pc, #(0x[0-9a-f]+|\d+)\]', i.op_str)
        if i.mnemonic == 'ldr' and m:
            t = ((i.address + 4) & ~3) + int(m.group(1), 0) - BASE
            if 0 <= t < len(fw) - 3: extra = '   ; =0x%08x' % struct.unpack_from('<I', fw, t)[0]
        print('%05x  %-6s %s%s' % (i.address, i.mnemonic, i.op_str, extra))
