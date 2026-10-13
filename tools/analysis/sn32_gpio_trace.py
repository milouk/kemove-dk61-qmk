#!/usr/bin/env python3
"""List GPIO and peripheral register accesses in an SN32F24xB image by constant propagation.

Usage: sn32_gpio_trace.py IMAGE.bin START_HEX END_HEX
"""
import sys, struct, re
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_MODE_MCLASS
from capstone.arm import *
fw = open(sys.argv[1], 'rb').read()
lo, hi = int(sys.argv[2], 16), int(sys.argv[3], 16)
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB | CS_MODE_MCLASS); md.detail = True; md.skipdata = True
P = {0x40044000:'GPIOA',0x40046000:'GPIOB',0x40048000:'GPIOC',0x4004A000:'GPIOD',0x4001C000:'SPI0',0x40042000:'PFPA',0x4005E000:'SYS1',0x40060000:'SYS0',0x40000000:'CT16B0',0x40002000:'CT16B1',0x40014000:'UART1',0x40032000:'PMU'}
GR = {0:'DATA',4:'MODE',8:'CFG',0xc:'IS',0x10:'IBS',0x14:'IEV',0x18:'IE',0x1c:'RIS',0x20:'IC',0x24:'BSET',0x28:'BCLR'}
reg = {}
def name(v):
    for b, n in P.items():
        if b <= v < b + 0x1000: return n, v - b
    return None, None
for i in md.disasm(fw[lo:hi], lo):
    if i.id == 0: continue
    ops = i.operands if hasattr(i, 'operands') else []
    m = i.mnemonic
    try:
        if m == 'ldr' and len(ops) == 2 and ops[1].type == ARM_OP_MEM and ops[1].mem.base == ARM_REG_PC:
            t = ((i.address + 4) & ~3) + ops[1].mem.disp
            reg[ops[0].reg] = struct.unpack_from('<I', fw, t)[0]; continue
        if m == 'movs' and len(ops) == 2 and ops[1].type == ARM_OP_IMM:
            reg[ops[0].reg] = ops[1].imm; continue
        if m in ('lsls','lsrs') and len(ops) == 3 and ops[2].type == ARM_OP_IMM and ops[1].reg in reg:
            v = reg[ops[1].reg]; reg[ops[0].reg] = (v << ops[2].imm) & 0xffffffff if m == 'lsls' else v >> ops[2].imm; continue
        if m == 'adds' and len(ops) == 2 and ops[1].type == ARM_OP_IMM and ops[0].reg in reg:
            reg[ops[0].reg] = (reg[ops[0].reg] + ops[1].imm) & 0xffffffff; continue
        if m in ('mov','movs') and len(ops) == 2 and ops[1].type == ARM_OP_REG:
            if ops[1].reg in reg: reg[ops[0].reg] = reg[ops[1].reg]
            else: reg.pop(ops[0].reg, None)
            continue
        if m in ('str','strh','strb','ldr','ldrh','ldrb') and len(ops) == 2 and ops[1].type == ARM_OP_MEM:
            b = ops[1].mem.base
            if b in reg and ops[1].mem.index == 0:
                n, off = name(reg[b] + ops[1].mem.disp)
                if n:
                    rn = GR.get(off, hex(off)) if n.startswith('GPIO') else hex(off)
                    val = reg.get(ops[0].reg) if m.startswith('str') else None
                    print('%05x %-4s %s.%-5s %s' % (i.address, m, n, rn, ('= 0x%x' % val) if val is not None else ('(var)' if m.startswith('str') else '')))
            if m.startswith('ldr'): reg.pop(ops[0].reg, None)
            continue
        if m in ('orrs','bics','ands','eors','subs','muls','asrs','rsbs','mvns','adcs','sbcs','uxtb','uxth','sxtb','sxth'):
            # track simple orrs/bics with known values for masks
            if len(ops) == 2 and ops[0].reg in reg and ops[1].type == ARM_OP_REG and ops[1].reg in reg and m in ('orrs','bics','ands'):
                a, b2 = reg[ops[0].reg], reg[ops[1].reg]
                reg[ops[0].reg] = {'orrs': a | b2, 'bics': a & ~b2, 'ands': a & b2}[m] & 0xffffffff
                print('%05x %-4s r%d mask 0x%x' % (i.address, m, ops[0].reg - ARM_REG_R0, b2)) if b2 < 0x10000 else None
            elif ops: reg.pop(ops[0].reg, None)
            continue
        if m in ('bl','blx','pop','bx'):
            for r in (ARM_REG_R0, ARM_REG_R1, ARM_REG_R2, ARM_REG_R3): reg.pop(r, None)
            if m in ('pop','bx'): reg.clear(); print('  --')
            continue
        if ops and ops[0].type == ARM_OP_REG and m not in ('cmp','tst','cmn','push','b','beq','bne'):
            reg.pop(ops[0].reg, None)
    except Exception as e:
        pass
