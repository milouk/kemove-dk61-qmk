#!/usr/bin/env python3
"""Update the QMK application through the OpenKemove DFU bootloader (feed:6969).

dfu-util fails against this bootloader on macOS: it always sends SET_INTERFACE, the
bootloader stalls it, and macOS reports that as an error. This talks DFU directly.

Usage: sfdfu.py IMAGE.bin [--libusb PATH_TO_LIBUSB_DYLIB]

Sequence: download, manifest (flushes the bootloader's last partial 1 KB buffer),
read the application back and compare, then reset into it. If the read-back does
not match, the keyboard is left in the bootloader.
"""
import argparse
import sys
import time

import usb.backend.libusb1
import usb.core

VID, PID = 0xFEED, 0x6969
APP_MAX = 0x10000 - 0x2000

DFU_DETACH, DFU_DNLOAD, DFU_UPLOAD, DFU_GETSTATUS, DFU_CLRSTATUS, DFU_ABORT = 0, 1, 2, 3, 4, 6
OUT, IN = 0x21, 0xA1

IDLE, DNLOAD_SYNC, DNBUSY, DNLOAD_IDLE = 2, 3, 4, 5
MANIFEST_SYNC, MANIFEST, MANIFEST_WAIT_RESET, ERROR = 6, 7, 8, 10


class Bootloader:
    def __init__(self, dev):
        self.dev = dev
        self.block = self._transfer_size()

    def _transfer_size(self):
        # wTransferSize lives in the DFU functional descriptor, after the interface descriptor.
        raw = bytes(self.dev.ctrl_transfer(0x80, 6, 0x0200, 0, 64))
        i = 0
        while i + 1 < len(raw):
            if raw[i + 1] == 0x21 and raw[i] >= 7:
                return raw[i + 5] | (raw[i + 6] << 8)
            i += raw[i] or len(raw)
        raise SystemExit('no DFU functional descriptor found')

    def status(self):
        raw = bytes(self.dev.ctrl_transfer(IN, DFU_GETSTATUS, 0, 0, 6, timeout=5000))
        poll_ms = raw[1] | (raw[2] << 8) | (raw[3] << 16)
        return raw[0], poll_ms, raw[4]

    def wait_for(self, wanted):
        for _ in range(200):
            status, poll_ms, state = self.status()
            if state == wanted:
                return
            if state == ERROR or status != 0:
                raise SystemExit('bootloader reported an error (status %d, state %d)' % (status, state))
            time.sleep(max(poll_ms, 5) / 1000)
        raise SystemExit('timed out waiting for bootloader state %d' % wanted)

    def abort(self):
        self.dev.ctrl_transfer(OUT, DFU_ABORT, 0, 0, None, timeout=5000)

    def download(self, image):
        self.abort()
        total = len(image)
        for n, off in enumerate(range(0, total, self.block)):
            self.dev.ctrl_transfer(OUT, DFU_DNLOAD, n, 0, image[off:off + self.block], timeout=5000)
            self.wait_for(DNLOAD_IDLE)
            print('\rwriting  %5d / %d bytes' % (min(off + self.block, total), total), end='', flush=True)
        print()
        # Zero-length download ends the transfer; the one status request after it runs the manifest
        # step. A second status request would reset the board, so ask exactly once.
        self.dev.ctrl_transfer(OUT, DFU_DNLOAD, 0, 0, None, timeout=5000)
        status, _, state = self.status()
        if status != 0 or state != MANIFEST:
            raise SystemExit('manifest did not start (status %d, state %d)' % (status, state))
        time.sleep(0.2)

    def upload(self, length):
        self.abort()
        data = b''
        while len(data) < length:
            chunk = bytes(self.dev.ctrl_transfer(IN, DFU_UPLOAD, 0, 0, self.block, timeout=5000))
            if not chunk:
                break
            data += chunk
            print('\rreading  %5d / %d bytes' % (min(len(data), length), length), end='', flush=True)
        print()
        self.abort()
        return data[:length]

    def reset(self):
        try:
            self.dev.ctrl_transfer(OUT, DFU_DETACH, 0, 0, None, timeout=2000)
        except usb.core.USBError:
            pass  # the device resets before it can answer


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('image')
    ap.add_argument('--libusb', help='path to libusb-1.0 dylib (default: system search)')
    args = ap.parse_args()

    image = open(args.image, 'rb').read()
    if not 0 < len(image) <= APP_MAX:
        raise SystemExit('image size %d is outside 1..%d' % (len(image), APP_MAX))

    backend = usb.backend.libusb1.get_backend(find_library=(lambda _: args.libusb) if args.libusb else None)
    dev = usb.core.find(idVendor=VID, idProduct=PID, backend=backend)
    if dev is None:
        raise SystemExit('no bootloader found (hold Esc while plugging in)')

    bl = Bootloader(dev)
    print('bootloader found, transfer size %d' % bl.block)
    bl.download(image)
    back = bl.upload(len(image))
    if back != image:
        diff = next((i for i, (a, b) in enumerate(zip(back, image)) if a != b), min(len(back), len(image)))
        raise SystemExit('VERIFY FAILED at offset 0x%x; keyboard left in the bootloader' % diff)
    print('verified: flash matches the image')
    bl.reset()
    print('reset sent; the keyboard should start the new firmware')


if __name__ == '__main__':
    sys.exit(main())
