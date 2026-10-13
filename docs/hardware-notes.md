# Hardware notes

What has been established about the Kemove DK61 family, and how each fact was obtained.

## Generations sharing the "DK61" name

| Generation | MCU | Wired USB ID | Product string | Vendor software |
| --- | --- | --- | --- | --- |
| Old ("Apex", Turing ODM) | NXP LPC11U35 | `1EA7:9006` Shadow, `1EA7:9005` Snowfox, `195D:2061` "61M" | Turing Gaming Keyboard | APEX-61M-UP |
| Two Mode | SN32F24xB (HFD2201KBA) | `0C45:8075` | KEMOVE DK61 | Kemove Control Hub |
| Three Mode (2.4G) | SN32F24xB | `0C45:8006` | KEMOVE K61 | Kemove Control Hub |

Source: Kemove's updaters and the APEX installer, unpacked with `tools/peres.py` and `tools/inno.py`.

## This board (old generation, PCB v1.5)

Confirmed on an actual unit.

- USB: `1EA7:9006`, device version `0x9015`, three HID interfaces (keyboard, `0xFF00/0xFF00`, `0xFF01/0x01`).
- PCB revision v1.5, read off the board.
- ISP entry: test pad `TP3` to `GND` at plug-in. The chip then enumerates as `1FC9:000F` "LPC1XXX IFLASH" with a 67.6 KB volume `CRP DISABLD`. (Tip from the OpenKemove Discord; it works.)
- Code read protection word at `0x2FC` is `0xFFFFFFFF` (off).

### Flash layout (from the full backup)

| Range | Contents |
| --- | --- |
| `0x0000–0x19F3` | Vendor bootloader (valid NXP vector checksum) |
| `0x2000–0xC4E3` | Application, firmware 9015 |
| above the application | Settings storage |
| `0xFFF8–0xFFFB` | ID stamp `1E A7 90 06` |

The application matches the archived stock 9015 image except one byte at `0x50FA` (`05` on this unit, `06` in the archive).

### Vendor update protocol

Read from the APEX software's JavaScript (`FWUpdate_Kemove`); not exercised on hardware.

- HID usage page `0xFF01`, usage `0x01`; feature report ID `0x07`, 264 bytes.
- `07 30` — enter the bootloader from the application, or leave it.
- `07 32` — erase the application.
- `07 34 <page>` — write one 256-byte page (payload at offset 8); 224 pages starting at `0x2000`.
- `07 01 FF` — reset to default profile.
- The updater stamps VID and PID, big-endian, at `0xFFF8`.
- The stock application enters the bootloader by erasing flash sector 15 through IAP and then resetting.

### Stock firmware sources

- APEX installer (`APEX-61M-UP_SETUP_20201110.exe`, Inno Setup): images 8060 and 9013 as Intel HEX.
- Archived `kemovebbs.com/softdownload/` on web.archive.org: 9013, 9015, 9017 (48-pin line) and 8058–8064 (64-pin line).

Firmware versions starting with 9 are the 48-pin MCU line; 8xxx is the 64-pin line.

## OpenKemove on v1.5

Flashed and running on this board.

- `kemove_lpc_dfu` has a `V1_5` target; DFU entry key is row `PIO1_25`, column `PIO1_28` (Esc).
- A build with GCC 13 at the default `-O2` produced 8456 bytes, more than the 8192 bytes reserved below the application at `0x2000`. Do not flash a bootloader image larger than 8192 bytes.
- thedelog's fork builds `kemove/snowfox/v1_5:default` at 29.6 KB. Its board file matches the stock firmware on every special-function pin (SPI to the LED drivers, UART to the Bluetooth module, battery sense, USB sense and connect) and on the Esc row and column.
- The bootloader stalls SET_INTERFACE, which makes `dfu-util` fail on macOS. Its transfer size is 64 bytes, it supports upload, and a second GETSTATUS after the manifest step resets the board.
- The Bluetooth status lights sit under Z, X and C (slots 1 to 3), purple; blinking means discovering or connecting.
- The fork's default keymap enables Auto Shift and tap-and-hold keys; with those on, the `.` key intermittently stuck repeating. Not seen since both were removed.
- The LPC11U35/401 has a 4 kB on-chip EEPROM (top 64 bytes reserved), read and written with IAP commands 62 and 61. The storage driver issues only those two commands, so it cannot alter flash or the bootloader. IAP calls need interrupts off, and the ROM's flash routines use the top 32 bytes of RAM, which the linker script leaves free.
- RAM is 8 kB. The VIA build leaves about 1.6 kB unused.

## Sonix generations (not this board)

Verified by disassembling and emulating Kemove's stock V1.07 image; kept here because the work is reusable.

- Reboot-to-bootloader command is the `hfd` type (SonixFlasherC `-r hfd`); bootloader ID `0C45:7040`.
- Matrix rows `D11, D10, D9, D8, D7`; columns `A8–A15, B0–B5`. DIP `D4` Mac/Win, `D5` high = Bluetooth.
- Bluetooth is the ITON module protocol on SPI0 (request line `A0`, interrupt line `A1`), supported by `drivers/bluetooth/iton_bt.c` in SonixQMK.
- Stock firmware boots in the Renode model from goyamamoto/KeychronK8-QMK.
- Sending an ITON command before the driver is initialised hangs the firmware; defer the first command about 500 ms.
- Three Mode adds five unknown control commands and has no `A1` interrupt.
