// Aselsan4000Panel, Copyright (C) 2026 onemikedelta
// Licensed under the GNU General Public License v3 or later; see LICENSE.

#ifndef ASELSAN4000_PANEL_H
#define ASELSAN4000_PANEL_H

#include <Arduino.h>

// Front panel of ASELSAN 4000-series radios. Everything hangs off one I2C bus:
//   PCF8576  LCD: eight 14-segment cells, a clock field and six icons
//   PCF8574  4x4 keypad
//   PCF8574  Tx/Rx LEDs, backlight, power switch / PTT / monitor inputs
//
// Display calls fill a buffer and reach the glass on show(); LED and backlight
// calls go out immediately.
class Aselsan4000Panel {
public:
  static const uint8_t CHAR_CELLS = 8;
  static const uint8_t RAM_SIZE = 20;

  // Icons are named after the symbols printed on the glass.
  enum Icon : uint8_t {
    IconBattery = 0,
    IconTone,
    IconCrypto,
    IconFault,
    IconHighPower,
    IconService,
    IconCount
  };

  enum Led : uint8_t { LedTx = 0, LedRx };

  // Labels key() returns for the four function keys; the rest return their own
  // character.
  static const char KeyMenu = 'M';
  static const char KeyEnter = 'E';
  static const char KeyScan = 'S';
  static const char KeyTone = 'K';

  Aselsan4000Panel(uint8_t lcdAddress = 0x38, uint8_t lcdSubAddress = 4,
                   uint8_t keypadAddress = 0x21, uint8_t controlAddress = 0x23);

  // Starts I2C, enables the display, lights the backlight and arms the keypad.
  void begin();

  // --- display ---
  void show();
  void clear();
  void print(const char *text);  // UTF-8, up to 8 characters
  void setChar(uint8_t cell, char c);
  void setCell(uint8_t cell, uint16_t segments);

  // hour 0..19 (only the tens digit '1' exists on the glass), minute 0..59.
  void setClock(uint8_t hour, uint8_t minute, bool colon = true);
  void clearClock();

  void setIcon(Icon icon, bool on);

  // Raw access for bits this driver does not name.
  void setByte(uint8_t index, uint8_t value);
  void setSegment(uint8_t index, uint8_t bit, bool on);
  uint8_t *buffer() { return _ram; }

  // 14-segment pattern for a character, 0 if unsupported.
  static uint16_t glyph(char c);
  static uint16_t glyphFor(uint16_t codepoint);

  // --- keypad, inputs, LEDs ---
  // Reads both expanders and re-arms them. Returns true when a key or an input
  // changed since the last call.
  bool poll();

  char key() const { return _keyLabel; }   // key label, 0 when none is down
  int8_t keyIndex() const { return _key; }  // row * 4 + column, -1 for none
  bool ptt() const { return _ptt; }         // active high
  bool monitor() const { return _monitor; }
  bool powerSwitch() const { return _power; }

  void setLed(Led led, bool on);
  void setBacklight(bool on);
  bool backlight() const { return _backlight; }

private:
  void sendLcdHeader();
  void writeControl();
  void armKeypad();
  void writeExpander(uint8_t address, uint8_t value);
  int readExpander(uint8_t address);

  uint8_t _lcdAddress;
  uint8_t _lcdSubAddress;
  uint8_t _keypadAddress;
  uint8_t _controlAddress;
  uint8_t _ram[RAM_SIZE];
  int8_t _key;
  char _keyLabel;
  bool _ptt;
  bool _monitor;
  bool _power;
  bool _ledTx;
  bool _ledRx;
  bool _backlight;
};

// Glyphs that have no ASCII code, for use with setCell(). The Turkish letters
// are also reachable through print(), which decodes UTF-8.
namespace Aselsan4000Glyph {
const uint16_t CCedilla = 0x0718;
const uint16_t GBreve = 0xA70A;
const uint16_t IDotted = 0x8118;
const uint16_t ODiaeresis = 0x234A;  // alt 0x870E: full O with the top-centre mark
const uint16_t SCedilla = 0x219A;
const uint16_t UDiaeresis = 0x030A;  // alt 0x8706: full U with the top-centre mark

const uint16_t ArrowUp = 0x1030;
const uint16_t ArrowDown = 0xC080;
const uint16_t ArrowLeft = 0x5040;
const uint16_t ArrowRight = 0x20A0;
const uint16_t ArrowUpLeft = 0x9488;
const uint16_t ArrowUpRight = 0xC02C;
const uint16_t ArrowDownLeft = 0x4330;
const uint16_t ArrowDownRight = 0x1192;
const uint16_t DataSent = ArrowUpRight;
const uint16_t DataReceived = ArrowDownLeft;
}

#endif
