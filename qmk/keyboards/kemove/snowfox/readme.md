# Kemove DK61 Snowfox / Shadow

A 61 key hotswap keyboard with USB and Bluetooth. This is the first generation, sold as "Turing Gaming Keyboard" with USB ID `1EA7:9005` (Snowfox) or `1EA7:9006` (Shadow). Later DK61 boards use a Sonix MCU and are not supported here.

* Keyboard Maintainer: [Michael Loukeris](https://github.com/milouk)
* Hardware Supported: Kemove DK61 Snowfox and Shadow, PCB v1.5 (NXP LPC11U35, CYW20730 Bluetooth module, 2x SLED1734x)
* Hardware Availability: [Kemove](https://www.kemove.com/)

Make example for this keyboard (after setting up your build environment):

    make kemove/snowfox/v1_5:default

See the [build environment setup](https://docs.qmk.fm/#/getting_started_build_tools) and the [make instructions](https://docs.qmk.fm/#/getting_started_make_guide) for more information. Brand new to QMK? Start with our [Complete Newbs Guide](https://docs.qmk.fm/#/newbs).

## Bootloader

The firmware is linked at `0x2000` and needs the [OpenKemove DFU bootloader](https://github.com/OpenKemove/kemove_lpc_dfu) in place of the stock one. Installing it and flashing are described at <https://github.com/milouk/kemove-dk61-qmk>.

Enter the bootloader in 2 ways:

* **Physical key**: hold Esc while plugging in the keyboard
* **Keycode in layout**: press the key mapped to `QK_BOOT`, Fn+B in the default keymap

## Keymap

The default keymap follows the stock layout and Fn shortcuts. The switch on the back selects the Windows or Mac base layer.

| Keys | Function |
| --- | --- |
| Fn+Backspace | Lighting on or off |
| Fn+\ | Next lighting mode |
| Fn+P, Fn+[ | Brightness down, up |
| Fn+Z, Fn+X, Fn+C | Bluetooth device 1, 2, 3. Hold 3 seconds to pair |
