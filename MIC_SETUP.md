INMP441 wiring for this board:

- `VDD` -> `3.3V`
- `GND` -> `GND`
- `SCK/BCLK` -> `GPIO36`
- `WS/LRCL` -> `GPIO35`
- `SD/DOUT` -> `GPIO37`
- `L/R` -> `GND`

Audio output note:

- The speaker path on this board shares `GPIO1`, `GPIO2`, `GPIO40` with the relay routing.
- Per the vendor documentation, move the `0R` resistors:
  - from `R25`, `R26`, `R27`
  - to `R21`, `R22`, `R23`

Current vendor UI mapping:

- `Screen 1` -> home/status/weather
- `Screen 10` -> settings (`Voice AI`, `Radio volume`, `Backlight`)
- `Screen 11` -> radio controls
- `Screen 12` -> assistant actions (`AI Chat`, `News Brief`, `Telegram`, `Mic Setup`)
- `Screen 13` -> clock
