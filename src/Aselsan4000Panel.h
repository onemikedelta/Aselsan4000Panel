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

  Aselsan4000Panel(uint8_t lcdAddress = 0x38, uint8_t lcdSubAddress = 4,
                   uint8_t keypadAddress = 0x21, uint8_t controlAddress = 0x23);

  // Starts I2C, enables the display, lights the backlight and arms the keypad.
  void begin();

  // --- display ---
  void show();
  void clear();
  void print(const char *text);
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

  // --- keypad, inputs, LEDs ---
  // Reads both expanders and re-arms them. Returns true when a key or an input
  // changed since the last call.
  bool poll();

  int8_t key() const { return _key; }  // 0..15, -1 when no key is down
  bool ptt() const { return _ptt; }    // active high
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
  bool _ptt;
  bool _monitor;
  bool _power;
  bool _ledTx;
  bool _ledRx;
  bool _backlight;
};

// Glyphs outside ASCII, for use with setCell().
namespace Aselsan4000Glyph {
const uint16_t CCedilla = 0x0718;
const uint16_t GBreve = 0xA70A;
const uint16_t SCedilla = 0x219A;
const uint16_t IDotted = 0x8118;

// Unverified candidates for the diaeresis letters; check them on the glass
// before relying on them.
// const uint16_t ODiaeresis = 0x870E;   // O plus the upper-centre dot
// const uint16_t UDiaeresis = 0x8706;   // U plus the upper-centre dot
// const uint16_t ODiaeresisLow = 0x63C2;  // small o plus both upper diagonals
}

#endif
