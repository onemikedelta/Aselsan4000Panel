// Shows text, the clock and the icons, reports keys and inputs over serial and
// mirrors PTT and monitor on the LEDs.
//
// For instant key response, wire the two expander INT lines (open drain, wired
// together) to D2 and call poll() from a pin-change flag instead of polling.
#include <Aselsan4000Panel.h>

Aselsan4000Panel panel;

const Aselsan4000Panel::Icon ICONS[] = {
    Aselsan4000Panel::IconBattery,   Aselsan4000Panel::IconTone,
    Aselsan4000Panel::IconCrypto,    Aselsan4000Panel::IconFault,
    Aselsan4000Panel::IconHighPower, Aselsan4000Panel::IconService};

uint8_t minutes = 0;
uint32_t lastTick = 0;

void redraw() {
  char text[9];
  if (panel.key() >= 0)
    snprintf(text, sizeof(text), "KEY %d", panel.key());
  else
    strcpy(text, "READY");

  panel.clear();
  panel.print(text);
  panel.setClock(10, minutes);
  panel.setIcon(ICONS[minutes % 6], true);
  panel.show();
}

void setup() {
  Serial.begin(57600);
  panel.begin();
  redraw();
}

void loop() {
  if (panel.poll()) {
    panel.setLed(Aselsan4000Panel::LedTx, panel.ptt());
    panel.setLed(Aselsan4000Panel::LedRx, panel.monitor());
    Serial.print(F("key="));
    Serial.print(panel.key());
    Serial.print(F(" ptt="));
    Serial.print(panel.ptt());
    Serial.print(F(" monitor="));
    Serial.print(panel.monitor());
    Serial.print(F(" power="));
    Serial.println(panel.powerSwitch());
    redraw();
  }

  if (millis() - lastTick >= 2000) {
    lastTick = millis();
    minutes = (minutes + 1) % 60;
    redraw();
  }
}
