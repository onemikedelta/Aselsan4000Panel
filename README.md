# Aselsan4000Panel

Arduino library for the front panel of ASELSAN 4000-series radios. One I2C bus
carries the whole panel, and this library drives all of it:

- **LCD** on a [PCF8576](https://www.nxp.com/docs/en/data-sheet/PCF8576D.pdf):
  eight 14-segment character cells, a three-digit clock field with a colon, and
  six annunciator icons (battery, tone, crypto, fault, high power, service)
- **Keypad** on a PCF8574: 4x4 matrix
- **Controls** on a second PCF8574: Tx and Rx LEDs, LCD backlight, and the power
  switch, PTT and monitor input lines

The complete 14-segment font is included and bench-verified, so the display
works like a normal character device.

## Usage

```cpp
#include <Aselsan4000Panel.h>

Aselsan4000Panel panel;

void setup() {
  panel.begin();            // starts Wire, enables the display, arms the keypad
  panel.print("CH 12");
  panel.setClock(9, 45);
  panel.setIcon(Aselsan4000Panel::IconHighPower, true);
  panel.show();             // nothing reaches the glass until show()
}

void loop() {
  if (!panel.poll()) return;            // true when a key or input changed
  panel.setLed(Aselsan4000Panel::LedTx, panel.ptt());
  if (panel.key() >= 0) {
    panel.print("KEY");
    panel.show();
  }
}
```

`begin()` calls `Wire.begin()` and sets the bus to 100 kHz. If you drive other
I2C devices yourself, this is safe to call anyway.

See [examples/PanelDemo](examples/PanelDemo/PanelDemo.ino).

## Wiring

| Panel | Arduino (Uno/Nano) |
| ----- | ------------------ |
| SDA   | A4                 |
| SCL   | A5                 |
| VDD   | 5V or 3V3 (match the panel) |
| VSS   | GND                |

4.7 kΩ pull-ups on SDA and SCL. The PCF8576 also needs its VLCD bias and
oscillator; on a self-contained panel module these are already on board.

Default addresses are `0x38` for the LCD (write byte `0x70`, device sub-address
`4`), `0x21` for the keypad and `0x23` for the controls. All four can be changed
in the constructor:

```cpp
Aselsan4000Panel panel(0x38, 4, 0x21, 0x23);
```

The two expanders have open-drain INT outputs. Wire them together to an
interrupt-capable pin (D2 on a Nano, active low) and you can call `poll()` the
moment something changes instead of polling in a loop.

## Installation

Copy this folder into your Arduino `libraries` directory, or in the IDE use
**Sketch → Include Library → Add .ZIP Library** with a zip of the repository.

## API

### Display

Display calls fill a buffer; `show()` pushes it to the panel in one I2C write.

| Call | Description |
| ---- | ----------- |
| `begin()` | start I2C, enable the display, arm the keypad |
| `show()` | push the buffer to the panel |
| `clear()` | blank the buffer (characters, clock and icons) |
| `print(text)` | write up to 8 characters, padding the rest with spaces |
| `setChar(cell, c)` | one character, cell 0–7, left to right |
| `setCell(cell, segments)` | raw 14-segment pattern, `high << 8 \| low` |
| `setClock(hour, minute, colon = true)` | clock field; hour 0–19, minute 0–59 |
| `clearClock()` | blank the clock field |
| `setIcon(icon, on)` | one annunciator |
| `setByte(index, value)` | raw display-RAM byte, index 0–19 |
| `setSegment(index, bit, on)` | raw display-RAM bit |
| `buffer()` | pointer to the 20-byte buffer |
| `glyph(c)` | font pattern for a character, `0` if unsupported |

Icons: `IconBattery`, `IconTone`, `IconCrypto`, `IconFault`, `IconHighPower`,
`IconService`.

Text and icons live in the same RAM bytes, but `print()`, `setChar()` and
`setCell()` preserve the icon bits, so the two never interfere.

### Keypad, inputs and LEDs

LED and backlight calls go out immediately; they are not buffered.

| Call | Description |
| ---- | ----------- |
| `poll()` | read both expanders and re-arm them; true if anything changed |
| `key()` | key held down as `row * 4 + column`, 0–15, or `-1` for none |
| `ptt()` | PTT line, active high |
| `monitor()` | monitor line, as read |
| `powerSwitch()` | power switch line, as read |
| `setLed(led, on)` | `LedTx` or `LedRx` |
| `setBacklight(on)` | backlight, on after `begin()` |
| `backlight()` | current backlight state |

`poll()` scans the matrix in two passes (rows driven, then columns) and re-arms
the keypad afterwards, which is also what clears the INT line. It reports a
single key; the matrix cannot tell combinations apart.

### Characters outside ASCII

`Aselsan4000Glyph` holds the Turkish glyphs that have no ASCII code, for use
with `setCell()`:

```cpp
panel.setCell(0, Aselsan4000Glyph::CCedilla);   // Ç
```

Available: `CCedilla`, `GBreve`, `SCedilla`, `IDotted`. Unverified candidates
for `Ö` and `Ü` are in the header, commented out.

## Panel map

Useful if you want to drive segments the library does not expose.
Display RAM is 20 bytes; a character cell is two bytes, high first.

```
byte  0  clock: minute ones (7-segment)
byte  1  clock: minute tens (7-segment), bit0 = colon
byte  2  clock: hour ones (7-segment)      physical order on glass: 2 1 0
byte  3  battery icon
bytes 4..19  character cells 0..7, (high, low) per cell
             cell 4 bit0 (byte 13) = clock hour-tens '1'
```

Segment bits within a cell, with `value = high << 8 | low`:

| Segment | Bit | Segment | Bit |
| ------- | --- | ------- | --- |
| A (top) | 3 | D (bottom) | 8 |
| B (upper right) | 2 | E (lower left) | 9 |
| C (lower right) | 1 | F (upper left) | 10 |
| G left (middle) | 6 | G right (middle) | 13 |
| H (upper left diag) | 7 | K (upper right diag) | 14 |
| L (lower left diag) | 5 | N (lower right diag) | 12 |
| J (upper centre) | 15 | M (lower centre) | 4 |

Bits 0 and 11 are not segments. Bit 0 carries an icon on cells 0–3
(crypto, fault, high power, service) and the hour-tens digit on cell 4;
bit 11 of cell 0 is the tone icon.

Expander pins:

```
keypad    P0..P3 rows, P4..P7 columns
controls  P0 Tx LED, P1 Rx LED, P2 backlight   (outputs, active low)
          P5 power switch, P6 PTT, P7 monitor  (inputs, held weak high)
```

A PCF8574 pin is quasi-bidirectional: writing 1 is a weak high (readable as an
input), writing 0 is a strong low.

## Known gaps

- The individual bits of the battery symbol are not mapped, so `IconBattery`
  lights the whole byte. Use `setSegment(3, bit, true)` to find them.
- The monitor icon has not been located on any byte yet.
- Bit 0 of cells 5–7 drives a row of dash segments whose meaning is unclear.
- Key numbering follows the matrix, not the printed labels.
- On the panel used for development, character cell 4 does not render the
  diagonal segments the way the other cells do, although they light on a full
  fill. Digits are correct there; full alphanumerics on that cell would need a
  per-cell bit map.

## License

GNU General Public License v3 or later, see [LICENSE](LICENSE). Anything built
on this library has to stay under the same license, so it cannot go into a
closed-source product. For a proprietary or commercial license, ask the author.
