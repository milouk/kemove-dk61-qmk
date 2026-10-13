#!/usr/bin/env python3
"""Look for ITON Bluetooth protocol bytes and peripheral references in an SN32F24xB image.

Usage: iton_fingerprint.py IMAGE.bin
"""
import sys, struct, collections
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_MODE_MCLASS
fw = open(sys.argv[1], 'rb').read()
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB | CS_MODE_MCLASS); md.skipdata = True
bases = {'SPI0':0x4001C000,'I2C0':0x40018000,'UART0':0x40016000,'UART1':0x40014000,'UART2':0x40012000,
         'GPIO0/A':0x40044000,'GPIO1/B':0x40046000,'GPIO2/C':0x40048000,'GPIO3/D':0x4004A000,'ADC':0x40026000,'USB':0x4005C000}
print('-- literal pool refs')
for n, b in bases.items():
    hits = [i for i in range(0, len(fw)-3, 4) if b <= struct.unpack_from('<I', fw, i)[0] < b + 0x100]
    print('  %-8s %3d %s' % (n, len(hits), ' '.join('%05x' % h for h in hits[:12])))
ins = list(md.disasm(fw, 0))
print('-- insns', len(ins))
# immediates of interest (iton protocol)
mov = collections.defaultdict(list); cmp_ = collections.defaultdict(list)
for i in ins:
    if i.mnemonic in ('movs', 'cmp') and '#' in i.op_str:
        try: v = int(i.op_str.split('#')[1], 0)
        except ValueError: continue
        (mov if i.mnemonic == 'movs' else cmp_)[v].append(i.address)
def show(tag, d, vals):
    for v in vals:
        print('  %s #0x%02x: %2d  %s' % (tag, v, len(d[v]), ' '.join('%05x' % a for a in d[v][:10])))
print('-- mcu->module command bytes (movs)'); show('movs', mov, [0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8])
print('-- control params (movs)'); show('movs', mov, [0x51,0x58,0x25,0x52,0x62,0x70,0x89,0x81,0x74,0x75,0x50,0x60,0x66,0x61,0x65,0x68])
print('-- module->mcu bytes (cmp)'); show('cmp ', cmp_, [0xB1,0xB6,0x5A,0x51,0x76,0x77,0x78,0x79,0x06,0x0A,0x07,0x0B])
