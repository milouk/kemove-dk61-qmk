#!/usr/bin/env python3
"""Decompress the data stream of an Inno Setup 6 installer into one blob.

Usage: inno_unpack.py SETUP.exe OUT_DIR
"""
import sys, lzma, struct
d = open(sys.argv[1], 'rb').read(); out = sys.argv[2]
offs = []; i = d.find(b'zlb\x1a')
while i >= 0: offs.append(i); i = d.find(b'zlb\x1a', i + 1)
print('chunks', len(offs), [hex(o) for o in offs[:8]])
n = 0
for o in offs:
    body = d[o + 4:]
    for kind in ('lzma2', 'lzma1'):
        try:
            if kind == 'lzma2':
                dict_size = 0xFFFFFFFF if body[0] == 40 else ((2 | (body[0] & 1)) << (body[0] // 2 + 11))
                dec = lzma.LZMADecompressor(lzma.FORMAT_RAW, filters=[{'id': lzma.FILTER_LZMA2, 'dict_size': dict_size}]); data = body[1:]
            else:
                p = body[0]; lc = p % 9; lp = (p // 9) % 5; pb = p // 45
                dict_size = struct.unpack_from('<I', body, 1)[0]
                dec = lzma.LZMADecompressor(lzma.FORMAT_RAW, filters=[{'id': lzma.FILTER_LZMA1, 'dict_size': dict_size, 'lc': lc, 'lp': lp, 'pb': pb}]); data = body[5:]
            res = b''
            try:
                while len(res) < 400 * 1024 * 1024 and not dec.eof:
                    chunk = dec.decompress(data[:1 << 22], 1 << 26); data = data[1 << 22:] if not dec.needs_input is False else data
                    if not chunk and not data: break
                    res += chunk
                    if dec.needs_input: data = data
                    else: continue
            except lzma.LZMAError as e:
                pass
            if len(res) > 4096:
                open('%s/chunk%d_%s.bin' % (out, n, kind), 'wb').write(res); print(hex(o), kind, 'ok', len(res)); n += 1; break
        except Exception as e:
            pass
