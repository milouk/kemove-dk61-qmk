# kemove-dk61-qmk

QMK for the old-generation Kemove DK61 Shadow / Snowfox (PCB v1.5), with a keymap that follows Kemove's stock layout.

> **Use at your own risk.** Flashing means opening the keyboard once and replacing Kemove's bootloader. Take the full backup described below first. This is a work in progress.

## Is this your board?

"DK61" covers several unrelated designs. This repo is for the first one.

| Generation | USB ID (wired) | MCU | Covered here |
| --- | --- | --- | --- |
| Old ("Turing Gaming Keyboard") | `1EA7:9006` Shadow, `1EA7:9005` Snowfox | NXP LPC11U35 | Yes, PCB v1.5 |
| Two Mode | `0C45:8075` | Sonix SN32F24xB | Notes only |
| Three Mode (2.4G) | `0C45:8006` | Sonix SN32F24xB | Notes only |

The PCB revision is printed on the board. v1.4 shares the v1.5 pin layout but is untested here; v1.2 and v3.0 need different builds.

## Status

| | |
| --- | --- |
| USB typing, stock layout | Working |
| Updates over USB, no disassembly | Working |
| Fn layer, lighting, Bluetooth, battery | Built, not yet tested |
| Saved settings (on-chip EEPROM), VIA keymap editing | Working |
| Lighting speed/colour keys, profiles, Fn lock, factory reset, battery display, VIA lighting control | Missing |

Kemove's APEX software does not talk to QMK.

## What is in here

- `qmk/keymaps/stock/` — stock layout and Fn shortcuts, from Kemove's K61 manual. Fn+B enters the bootloader.
- `qmk/keymaps/via/` — the same keymap with VIA enabled.
- `qmk/via/` — VIA definition; load it in VIA's Design tab.
- `qmk/patches/` — changes to the upstream QMK fork and to the bootloader.
- `tools/sfdfu.py` — firmware updater for the OpenKemove bootloader. `dfu-util` fails with it on macOS.
- `tools/analysis/` — scripts used to unpack Kemove's installers and inspect firmware images.
- `docs/hardware-notes.md` — what was verified about the hardware, and how.

No Kemove firmware is included.

## Build

Needs `arm-none-eabi-gcc` and the QMK CLI.

```sh
# QMK
git clone --recursive https://github.com/thedelog/Kemove-Snowfox-Media_keys_on_BLE.git qmk_firmware
cd qmk_firmware
git apply ../qmk/patches/000*.patch
git -C lib/chibios-contrib apply ../../../qmk/patches/chibios-contrib-lpc11uxx-usb-fixes.patch
cp -r ../qmk/keymaps/* keyboards/kemove/snowfox/keymaps/
make kemove/snowfox/v1_5:stock   # or :via

# Bootloader
git clone --recursive https://github.com/OpenKemove/kemove_lpc_dfu.git
cd kemove_lpc_dfu
git apply ../qmk/patches/kemove_lpc_dfu-static-inline.patch
make V1_5 USE_OPT="-Os -g -fomit-frame-pointer -falign-functions=16"
```

The bootloader must be 8192 bytes or smaller. Built at the default `-O2` with a current GCC it is not.

## First install

1. Remove the keycaps, switches and PCB screws, lift the PCB and unplug the battery.
2. Short test pad `TP3` to `GND` while plugging in USB. A 64 KB drive named `CRP DISABLD` appears, holding `firmware.bin`.
3. **Back up:** `cat "/Volumes/CRP DISABLD/firmware.bin" > stock_backup.bin`. Keep it; it is the only way back to stock.
4. Build one 64 KB image: the bootloader at `0x0000`, the QMK image at `0x2000`, `0xFF` everywhere else.
5. Write it in place and unmount:

   ```sh
   dd if=image_64k.bin of="/Volumes/CRP DISABLD/firmware.bin" conv=notrunc bs=512
   sync && diskutil unmount "/Volumes/CRP DISABLD"
   ```

6. Repeat step 2 and compare `firmware.bin` with your image, then replug normally.

On macOS use Terminal only; do not open the drive in Finder.

Never write an image whose 32-bit word at `0x2FC` is `0x12345678`, `0x87654321`, `0x43218765` or `0x4E697370`. Those turn on the chip's code read protection, and the last two disable this recovery mode for good.

## Update

1. Press Fn+B, or hold Esc while plugging in. The keyboard goes dark and shows up as `feed:6969`.
2. `pip install pyusb`, then `tools/sfdfu.py firmware.bin`. It writes, reads back, verifies and restarts the keyboard.

## Back to stock

Repeat step 2 of the install and write your backup with the same `dd` command.

## Credits

- [OpenKemove](https://openkemove.github.io/) — the LPC11Uxx QMK port and DFU bootloader.
- [hftsai256](https://github.com/hftsai256/openkemove_qmk_firmware) — v1.4/v1.5 support and three Bluetooth devices.
- [thedelog](https://github.com/thedelog/Kemove-Snowfox-Media_keys_on_BLE) — media keys over Bluetooth; the tree this builds on.
- The OpenKemove Discord, for the `TP3` tip.

## License

GPL-2.0-or-later, as QMK.
