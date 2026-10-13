#!/usr/bin/env python3
"""List a PE file's resources and string tables, saving large resources to OUT_DIR.

Usage: pe_resources.py FILE.exe OUT_DIR
"""
import struct, sys, os, hashlib
path, outdir = sys.argv[1], sys.argv[2]
d = open(path, 'rb').read()
pe = struct.unpack_from('<I', d, 0x3c)[0]
nsec = struct.unpack_from('<H', d, pe + 6)[0]
optsz = struct.unpack_from('<H', d, pe + 20)[0]
opt = pe + 24
magic = struct.unpack_from('<H', d, opt)[0]
ddir = opt + (96 if magic == 0x10b else 112)
res_rva, res_sz = struct.unpack_from('<II', d, ddir + 2 * 8)
secs = []
for i in range(nsec):
    o = opt + optsz + i * 40
    name = d[o:o+8].rstrip(b'\0').decode('latin1')
    vsz, va, rsz, rptr = struct.unpack_from('<IIII', d, o + 8)
    secs.append((name, va, vsz, rptr, rsz))
def off(rva):
    for n, va, vsz, rptr, rsz in secs:
        if va <= rva < va + max(vsz, rsz):
            return rptr + rva - va
    raise ValueError(rva)
base = off(res_rva)
def name_of(e):
    if e & 0x80000000:
        o = base + (e & 0x7fffffff)
        n = struct.unpack_from('<H', d, o)[0]
        return d[o+2:o+2+2*n].decode('utf-16le')
    return str(e)
def walk(o, pathp):
    nn, ni = struct.unpack_from('<HH', d, o + 12)
    for i in range(nn + ni):
        nm, to = struct.unpack_from('<II', d, o + 16 + 8 * i)
        p = pathp + [name_of(nm)]
        if to & 0x80000000:
            walk(base + (to & 0x7fffffff), p)
        else:
            rva, sz, cp, _ = struct.unpack_from('<IIII', d, base + to)
            yield_list.append((p, off(rva), sz))
yield_list = []
walk(base, [])
for p, o, sz in yield_list:
    t = p[0]
    if t in ('1','2','3','12','14','5','4','6','9','16','24','241','240'):  # cursors/bitmaps/icons/dialogs/menus/strings etc
        if t != '6': continue
    blob = d[o:o+sz]
    if t == '6':
        # string table
        i = 0; idx = (int(p[1]) - 1) * 16
        while i < len(blob):
            n = struct.unpack_from('<H', blob, i)[0]; i += 2
            if n:
                print('STR', idx, repr(blob[i:i+2*n].decode('utf-16le')))
            i += 2 * n; idx += 1
        continue
    fn = '_'.join(p).replace('/', '_')
    print('RES', p, sz, hashlib.sha256(blob).hexdigest()[:16], blob[:16].hex())
    if sz > 1024:
        open(os.path.join(outdir, fn + '.bin'), 'wb').write(blob)
