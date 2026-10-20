// Aselsan4000Panel, Copyright (C) 2026 onemikedelta
// Licensed under the GNU General Public License v3 or later; see LICENSE.

#include "Aselsan4000Panel.h"
#include <Wire.h>

// Display RAM (20 bytes, big-endian pairs):
//   0..2   clock digits: minute ones, minute tens (+colon on bit0), hour ones
//   3      battery icon
//   4..19  eight character cells, two bytes each (high, low)
// Within a character cell bit0 and bit11 are not segments; they carry icons.
static const uint8_t CELL_BASE = 4;
static const uint8_t ICON_MASK_HIGH = 0x08;
static const uint8_t ICON_MASK_LOW = 0x01;

static const uint8_t CMD_MODE_ON = 0xC8;  // display on, 1/3 bias
static const uint8_t CMD_NO_BLINK = 0xF0;
static const uint8_t CMD_POINTER_0 = 0x00;

// Control expander: P0/P1 LEDs and P2 backlight are outputs, active low;
// P5 power switch, P6 PTT, P7 monitor are inputs, held weak high.
static const uint8_t CTRL_LED_TX = 0x01;
static const uint8_t CTRL_LED_RX = 0x02;
static const uint8_t CTRL_BACKLIGHT = 0x04;
static const uint8_t CTRL_POWER = 0x20;
static const uint8_t CTRL_PTT = 0x40;
static const uint8_t CTRL_MONITOR = 0x80;

// Keypad expander: rows on P0..P3, columns on P4..P7. Writing 0xF0 drives the
// rows low and leaves the columns weak high, so a press pulls its column low
// (and the INT line with it); 0x0F swaps the two to read the row.
static const uint8_t KEYPAD_ARM_ROWS = 0xF0;
static const uint8_t KEYPAD_ARM_COLUMNS = 0x0F;

// Key labels by row * 4 + column.
static const char KEY_LABEL[16] PROGMEM = {
    '1', '2', '3', Aselsan4000Panel::KeyMenu,
    '4', '5', '6', Aselsan4000Panel::KeyEnter,
    '7', '8', '9', Aselsan4000Panel::KeyScan,
    '*', '0', '#', Aselsan4000Panel::KeyTone};

// Clock digits are plain 7-segment.
static const uint8_t SEG7[10] PROGMEM = {
    0xBE, 0xA0, 0x7C, 0xF8, 0xE2, 0xDA, 0xDE, 0xB0, 0xFE, 0xFA};

// 14-segment patterns for ASCII 0x20..0x5F, high byte << 8 | low byte.
// The radio itself also emits a short-middle-bar E (0x0748) and F (0x0648);
// accept those too if you decode traffic coming from the radio.
static const uint16_t FONT[] PROGMEM = {
    0x0000, 0x0000, 0x0000, 0x57AE, 0x0000, 0x0000, 0x0000, 0x0000,
    0x5000, 0x00A0, 0xF0F0, 0xA050, 0x0000, 0x2040, 0x0000, 0x4020,
    0x472E, 0x0006, 0x4348, 0x210E, 0x2446, 0x254A, 0x274A, 0x000E,
    0x274E, 0x254E, 0x0000, 0x0000, 0x0000, 0x2140, 0x0000, 0x0000,
    0x0000, 0x264E, 0xA11E, 0x0708, 0x811E, 0x2748, 0x2648, 0x270A,
    0x2646, 0x8118, 0x0306, 0x5640, 0x0700, 0x4686, 0x1686, 0x070E,
    0x264C, 0x170E, 0x364C, 0x218A, 0x8018, 0x0706, 0x4620, 0x1626,
    0x50A0, 0x4090, 0x4128, 0x0000, 0x1080, 0x0000, 0x0000, 0x0100};
static const uint16_t FONT_FIRST = ' ';
static const uint16_t FONT_LAST = '_';

static const uint8_t ICON_BYTE[Aselsan4000Panel::IconCount] PROGMEM = {
    3, 4, 5, 7, 9, 11};
static const uint8_t ICON_BIT[Aselsan4000Panel::IconCount] PROGMEM = {
    0x10, 0x08, 0x01, 0x01, 0x01, 0x01};

static int8_t lowestBit(uint8_t mask) {
  for (int8_t i = 0; i < 4; i++)
    if (mask & (1 << i)) return i;
  return -1;
}

Aselsan4000Panel::Aselsan4000Panel(uint8_t lcdAddress, uint8_t lcdSubAddress,
                                   uint8_t keypadAddress, uint8_t controlAddress)
    : _lcdAddress(lcdAddress),
      _lcdSubAddress(lcdSubAddress & 0x07),
      _keypadAddress(keypadAddress),
      _controlAddress(controlAddress),
      _key(-1),
      _keyLabel(0),
      _ptt(false),
      _monitor(false),
      _power(false),
      _ledTx(false),
      _ledRx(false),
      _backlight(true) {
  memset(_ram, 0, RAM_SIZE);
}

void Aselsan4000Panel::begin() {
  Wire.begin();
  Wire.setClock(100000);
  Wire.beginTransmission(_lcdAddress);
  sendLcdHeader();
  Wire.endTransmission();
  clear();
  show();
  writeControl();
  armKeypad();
  poll();  // prime the key and input state
}

// --- display ---

void Aselsan4000Panel::sendLcdHeader() {
  Wire.write(0xE0 | _lcdSubAddress);  // device select
  Wire.write(CMD_MODE_ON);
  Wire.write(CMD_NO_BLINK);
}

void Aselsan4000Panel::show() {
  Wire.beginTransmission(_lcdAddress);
  sendLcdHeader();
  Wire.write(CMD_POINTER_0);
  for (uint8_t i = 0; i < RAM_SIZE; i++) Wire.write(_ram[i]);
  Wire.endTransmission();
}

void Aselsan4000Panel::clear() { memset(_ram, 0, RAM_SIZE); }

void Aselsan4000Panel::setCell(uint8_t cell, uint16_t segments) {
  if (cell >= CHAR_CELLS) return;
  uint8_t high = CELL_BASE + 2 * cell;
  // Keep the icon bits that share this cell.
  _ram[high] = (_ram[high] & ICON_MASK_HIGH) |
               (uint8_t)((segments >> 8) & (uint8_t)~ICON_MASK_HIGH);
  _ram[high + 1] = (_ram[high + 1] & ICON_MASK_LOW) |
                   (uint8_t)(segments & (uint8_t)~ICON_MASK_LOW);
}

void Aselsan4000Panel::setChar(uint8_t cell, char c) { setCell(cell, glyph(c)); }

void Aselsan4000Panel::print(const char *text) {
  const uint8_t *p = (const uint8_t *)text;
  uint8_t cell = 0;
  while (*p && cell < CHAR_CELLS) {
    uint16_t cp = *p++;
    if (cp >= 0xC0) {  // start of a UTF-8 sequence
      uint8_t extra = (cp >= 0xE0) ? 2 : 1;
      cp &= (extra == 2) ? 0x0F : 0x1F;
      while (extra-- && (*p & 0xC0) == 0x80) cp = (cp << 6) | (*p++ & 0x3F);
    }
    setCell(cell++, glyphFor(cp));
  }
  while (cell < CHAR_CELLS) setCell(cell++, 0);
}

void Aselsan4000Panel::setClock(uint8_t hour, uint8_t minute, bool colon) {
  _ram[2] = pgm_read_byte(&SEG7[hour % 10]);
  _ram[1] = pgm_read_byte(&SEG7[(minute / 10) % 10]) | (colon ? 0x01 : 0x00);
  _ram[0] = pgm_read_byte(&SEG7[minute % 10]);
  setSegment(13, 0, hour >= 10);
}

void Aselsan4000Panel::clearClock() {
  _ram[0] = _ram[1] = _ram[2] = 0;
  setSegment(13, 0, false);
}

void Aselsan4000Panel::setIcon(Icon icon, bool on) {
  if (icon >= IconCount) return;
  uint8_t index = pgm_read_byte(&ICON_BYTE[icon]);
  uint8_t mask = pgm_read_byte(&ICON_BIT[icon]);
  if (on)
    _ram[index] |= mask;
  else
    _ram[index] &= ~mask;
}

void Aselsan4000Panel::setByte(uint8_t index, uint8_t value) {
  if (index < RAM_SIZE) _ram[index] = value;
}

void Aselsan4000Panel::setSegment(uint8_t index, uint8_t bit, bool on) {
  if (index >= RAM_SIZE || bit > 7) return;
  if (on)
    _ram[index] |= (1 << bit);
  else
    _ram[index] &= ~(1 << bit);
}

uint16_t Aselsan4000Panel::glyph(char c) { return glyphFor((uint8_t)c); }

uint16_t Aselsan4000Panel::glyphFor(uint16_t codepoint) {
  if (codepoint >= 'a' && codepoint <= 'z') codepoint -= 32;
  if (codepoint >= FONT_FIRST && codepoint <= FONT_LAST)
    return pgm_read_word(&FONT[codepoint - FONT_FIRST]);
  switch (codepoint) {
    case 0x00C7:
    case 0x00E7:
      return Aselsan4000Glyph::CCedilla;
    case 0x011E:
    case 0x011F:
      return Aselsan4000Glyph::GBreve;
    case 0x0130:
    case 0x0131:
      return Aselsan4000Glyph::IDotted;
    case 0x00D6:
    case 0x00F6:
      return Aselsan4000Glyph::ODiaeresis;
    case 0x015E:
    case 0x015F:
      return Aselsan4000Glyph::SCedilla;
    case 0x00DC:
    case 0x00FC:
      return Aselsan4000Glyph::UDiaeresis;
  }
  return 0;
}

// --- keypad, inputs, LEDs ---

void Aselsan4000Panel::writeExpander(uint8_t address, uint8_t value) {
  Wire.beginTransmission(address);
  Wire.write(value);
  Wire.endTransmission();
}

int Aselsan4000Panel::readExpander(uint8_t address) {
  if (Wire.requestFrom(address, (uint8_t)1) != 1) return -1;
  return Wire.read();
}

// Writing the control expander also releases its inputs and clears its INT.
void Aselsan4000Panel::writeControl() {
  uint8_t value = 0xFF;  // every pin weak high: inputs readable, outputs off
  if (_ledTx) value &= ~CTRL_LED_TX;
  if (_ledRx) value &= ~CTRL_LED_RX;
  if (_backlight) value &= ~CTRL_BACKLIGHT;
  writeExpander(_controlAddress, value);
}

void Aselsan4000Panel::armKeypad() {
  writeExpander(_keypadAddress, KEYPAD_ARM_ROWS);
}

bool Aselsan4000Panel::poll() {
  writeExpander(_keypadAddress, KEYPAD_ARM_ROWS);
  int columns = readExpander(_keypadAddress);
  writeExpander(_keypadAddress, KEYPAD_ARM_COLUMNS);
  int rows = readExpander(_keypadAddress);
  armKeypad();

  int control = readExpander(_controlAddress);
  writeControl();

  int8_t key = -1;
  if (columns >= 0 && rows >= 0) {
    int8_t column = lowestBit((~(uint8_t)columns >> 4) & 0x0F);
    int8_t row = lowestBit(~(uint8_t)rows & 0x0F);
    if (column >= 0 && row >= 0) key = row * 4 + column;
  }

  uint8_t lines = (control < 0) ? 0xFF : (uint8_t)control;
  bool ptt = lines & CTRL_PTT;
  bool monitor = lines & CTRL_MONITOR;
  bool power = lines & CTRL_POWER;

  bool changed = key != _key || ptt != _ptt || monitor != _monitor ||
                 power != _power;
  _key = key;
  _keyLabel = (key < 0) ? 0 : (char)pgm_read_byte(&KEY_LABEL[key]);
  _ptt = ptt;
  _monitor = monitor;
  _power = power;
  return changed;
}

void Aselsan4000Panel::setLed(Led led, bool on) {
  if (led == LedTx)
    _ledTx = on;
  else
    _ledRx = on;
  writeControl();
}

void Aselsan4000Panel::setBacklight(bool on) {
  _backlight = on;
  writeControl();
}
