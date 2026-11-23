# FM-1 CRAFT: the wire

Everything goes out over USB-MIDI on **channel 16**. The FM-1 makes no sound.

| Control | Message |
|---|---|
| Keys (27, F3..G5) | Note on / off, note 53 + key (F3 = 53, G5 = 79), velocity 100 |
| KNOB 1–4 | CC 20–23, relative (two's complement: 1..63 clockwise, 127..65 counter-clockwise) |
| SELECT · ALGORITHM · PRESETS | CC 24 · 25 · 26, relative |
| MASTER | CC 27, absolute 0..127 |
| Page shown | CC 28: 0 POS, 1 SIZE, 2 STYLE, 3 TEXT (sent at start and on every change) |
| Buttons | CC 102 + index, 127 down / 0 up. Index: FX 0, SEL 1, ENV 2, LFO 3, EDIT 4, GLO 5, HOME 6, SAVE 7, ARP 8, SEQ 9, PLAY 10, REC 11, OCT− 12, OCT+ 13 |

Page buttons: HOME = POS, ENV = SIZE, LFO = STYLE, ARP = TEXT (their CCs go out too).

## From the computer: SysEx `F0 7D 46 43 <cmd> … F7`

| cmd | Bytes after it | Does |
|---|---|---|
| `01` STATE | page, bar1..bar4 (0..100, `7F` = none), then NUL-ended ASCII strings: layer, value1..value4 | The screen: tabs, layer name, the four cards |
| `02` SAY | two NUL-ended strings | A message over the header for 1.3 s |
| `03` PING | — | The script is there |

Any frame keeps the link lit; with none for 3 s the header says NO LINK and the values clear.
