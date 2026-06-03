# eKairn
BLE generic beacon for Orienteering
--------------------------------------------------
Rev 7.b.x (Beta)  - 26/05/2026

[KNOWN ISSUES]
1) Command "A" not implemented.
2) Extra consumption when using ePaper. +1mA explained by the fact the SPI and the PIN are not in a properly defined state.
3) chip temperature measurement wrong (25°C is displayed instead).
4) Buzzer is active low (active Buzzer), wich create noise is not selected.

[TODO]
1) Implement "A" Command
2) Solve Issue number 2) by properly closing the SPI (see explaination)
3) Implement Buzzer (and MegaLED) generic GPIO function: No, ActiveHigh, ActiveLow, OpenDrain

[WHISH LIST]
1) RTC with wakeup on a due time. Cycling (for example operates 5H every 7 days).

--------------------------------------------------
Rev 7.a.x (Alpha)  - 19/05/2026

[KNOWN ISSUES]
1) Command "A" not implemented.
2) Extra consumption when using ePaper. +1mA explained by the fact the SPI and the PIN are not in a properly defined state.

[TODO]
1) Implement "A" Command
2) Solve Issue number 2) by properly closing the SPI (see explaination)
3) Measure chip temperature and display it with the voltage.

[WHISH LIST]
1) Change  command "L" and "l" into "L1" and "L0" so that we can have a generic switch case for the command instead of IF sequences.
2) When ERM key is 0000 enter directely in ERM mode when connecting.
3) When FAM key is 0000 enter directely in FAM mode when connecting.
4) RTC with wakeup on a due time. Cycling (for example operates 5H every 7 days).
5) Warm if temperature above 55°C
