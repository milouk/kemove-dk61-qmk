#!/usr/bin/env python3
"""List the pins an LPC11U35 image configures through IOCON (Intel HEX input).

Usage: lpc_iocon_pins.py IMAGE.hex
"""
import sys, struct, re
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_MODE_MCLASS
from capstone.arm import *
def load_hex(p):
    mem = {}; base = 0
    for line in open(p, errors='replace'):
        line = line.strip()
        if not line.startswith(':'): continue
        raw = bytes.fromhex(line[1:]); n, addr, typ = raw[0], (raw[1] << 8) | raw[2], raw[3]
        if typ == 4: base = ((raw[4] << 8) | raw[5]) << 16
        elif typ == 0:
            for i in range(n): mem[base + addr + i] = raw[4 + i]
    lo, hi = min(mem), max(mem); return lo, bytes(mem.get(a, 0xff) for a in range(lo, hi + 1))
BASE, fw = load_hex(sys.argv[1])
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB | CS_MODE_MCLASS); md.detail = True; md.skipdata = True
reg = {}; iocon = set(); bytepins = set(); wordpins = set()
for i in md.disasm(fw, BASE):
    if i.id == 0: reg.clear(); continue
    ops = i.operands; m = i.mnemonic
    if m == 'ldr' and len(ops) == 2 and ops[1].type == ARM_OP_MEM and ops[1].mem.base == ARM_REG_PC:
        t = ((i.address + 4) & ~3) + ops[1].mem.disp - BASE
        reg[ops[0].reg] = struct.unpack_from('<I', fw, t)[0] if 0 <= t < len(fw) - 3 else None; continue
    if m == 'adds' and len(ops) == 2 and ops[1].type == ARM_OP_IMM and reg.get(ops[0].reg) is not None:
        reg[ops[0].reg] = (reg[ops[0].reg] + ops[1].imm) & 0xffffffff; continue
    if m in ('str', 'strb', 'ldr', 'ldrb', 'strh', 'ldrh') and len(ops) == 2 and ops[1].type == ARM_OP_MEM and ops[1].mem.index == 0:
        b = reg.get(ops[1].mem.base)
        if b is not None:
            a = b + ops[1].mem.disp
            if 0x40044000 <= a < 0x400440E0: iocon.add((a - 0x40044000) // 4)
            if 0x50000000 <= a < 0x50000040: bytepins.add(a - 0x50000000)
            if 0x50001000 <= a < 0x50001100: wordpins.add((a - 0x50001000) // 4)
        if m.startswith('ldr') and ops[0].type == ARM_OP_REG: reg.pop(ops[0].reg, None)
        continue
    if m in ('bl', 'blx', 'bx', 'pop'): reg.clear(); continue
    if ops and ops[0].type == ARM_OP_REG and m not in ('cmp', 'tst', 'push', 'str', 'strb', 'strh'): reg.pop(ops[0].reg, None)
# IOCON index -> pin name (UM10462 table: PIO0_0..23 at idx 0..23, PIO1_0..31 at idx 24..55)
def nm(ix): return 'P0_%d' % ix if ix < 24 else 'P1_%d' % (ix - 24)
p = sorted(iocon)
print('IOCON pins configured:', ' '.join(nm(x) for x in p))
print('  port1 pins:', sorted(x - 24 for x in p if x >= 24))
allp = sorted(bytepins | wordpins)
print('GPIO pin registers touched:', ' '.join(('P0_%d' % x) if x < 32 else ('P1_%d' % (x - 32)) for x in allp))
