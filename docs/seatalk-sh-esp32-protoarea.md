# SeaTalk ↔ SH-ESP32 Proto-Area Subcircuit (opto-block version)

> **Status: superseded.** The current build uses two NPN transistors instead of
> the opto block: `seatalk-sh-esp32-npn.svg` (circuit) and
> `seatalk-sh-esp32-npn-protolayout.svg` (proto-area layout), firmware RX = GPIO27,
> TX = GPIO26, `invert=true`. This document is kept as a record of the opto design.
>
> **Corrected after reading the Hat Labs Rev 2.0.0 schematic** (opto sheet, page 8,
> <https://docs.hatlabs.fi/sh-esp32/revisions/assets/SH-ESP32-2.0.0-schema.pdf>).
> The first version of this document had three errors, each of which stops the
> interface working:
>
> 1. **ISO_OUT polarity.** ISO_OUT is the collector of an NPN (Q901, MMBT5551,
>    160 V) with a 3.9 kΩ pull-up (R904) to V_ISO. The opto between GPIO33 and Q901
>    inverts, so **GPIO33 LOW turns Q901 on and pulls ISO_OUT low**; GPIO33 HIGH
>    releases it. The old text said GPIO33 HIGH pulls DATA low.
> 2. **D_out was backwards.** With the band toward DATA, the diode blocks current
>    flowing from DATA into ISO_OUT, so TX could never pull the bus low. D_out is
>    now removed: ISO_OUT is an open-collector output and connects straight to DATA,
>    and R904 then adds to the bus pull-up.
> 3. **Firmware polarity.** Both opto stages are non-inverting (DATA high gives
>    GPIO35 high; GPIO33 high leaves DATA high), so SoftwareSerial needs
>    `invert=false`. The old text said `invert=true`, which would hold the bus low
>    whenever the ESP32 is idle.
>
> A fourth issue is bus loading: R_in 2.2 kΩ plus the on-board R905 2.2 kΩ draws
> about 2.5 mA from DATA into the opto LED. Against a 10 kΩ pull-up alone that
> holds DATA near 4.4 V at idle, which matches the 4.5 V measured during bring-up.
> R_in is now 5.6 kΩ (about 1.4 mA, the LED current Hat Labs designed for).
> Even so, DATA idles around 9 V rather than 12 V with only R_pull and R904 as
> pull-ups. That loading is the main reason the NPN version replaced this one.
>
> The schematic is Rev 2.0.0; the board is Rev 2.1.0. Confirm the output stage with
> a meter before relying on it: with GPIO33 held LOW, ISO_OUT to ISO_GND should read
> near 0 V.

Companion to `seatalk-sh-esp32-handoff.md`. Open questions resolved:

1. **Proto grid geometry** — confirmed from `sh-esp32_r2.0.0_top_render.jpg`
   and `sh-bottom.jpg`: columns **B…T**, rows **1…7**, 3.3V and GND corner
   rails. (Separate 7-pin block further left is the "I/O" header.)
   **Board is Hat Labs SH-ESP32 Revision 2.1.0** (per bottom-side silkscreen).
2. **Bus pull-up** — required. Replicates **R3 (10 kΩ)** from the original
   discrete board, which drove the SeaTalk line idle-high at +12 V and was
   proven working alongside the ST2000+. Use **R_pull = 10 kΩ to on-board +12 V**.
3. **RX + TX** — firmware both reads (compass heading, 0x84; echo, 0xAC) and
   writes (keypad commands, `sendCompass`). Keep both opto paths.

> **Note on "Protected Voltage" pads:** earlier drafts referenced SH-ESP32
> "Protected Voltage" pads. **This board revision does not have them.** Wherever
> a "protected +12 V" node was mentioned, use the board's actual on-board
> **+12 V** (the same bus 12 V that feeds the power connector). Same net.
>
> **Opto-pad names on Rev 2.1.0** (from `sh-bottom.jpg`): the opto block is
> labeled **ISO_OUT / ISO_IN / V_ISO / ISO_GND** on the bottom silkscreen
> (older docs call these OUT / IN / Vext / GND). Mapping used below:
> `ISO_OUT`=opto output (GPIO33), `ISO_IN`=opto input (GPIO35),
> **`V_ISO`=Vext** (supply for the output drive), `ISO_GND`=isolated-side ground.
> Use the bottom-side names to find the pads; they are unambiguous there.

## Power / isolation topology (single source: CAN-bus 12 V)

Only one 12 V source is available: the NMEA 2000 / CAN bus. The ST2000+ draws
**0.5–1.5 A in Auto** (handbook 81130-9-EN §1.2), far above the SH-ESP32's
**500 mA input polyfuse** and **0.8 A switcher**, so the pilot's motor current
**must not pass through the board**. Split the feed at the bus entry:

```
CAN bus +12V ──┬── ST2000+ (+12V)          0.5–1.5 A, fuse per Raymarine (~2–3 A)
               └── SH-ESP32 2-pin power (+) ESP32 only, <500 mA
CAN bus GND  ──┬── ST2000+ (GND)
               └── SH-ESP32 2-pin power (−)
CAN_H ───────────► SH-ESP32 4-pin CAN_H     NMEA 2000 data
CAN_L ───────────► SH-ESP32 4-pin CAN_L     NMEA 2000 data
```

- **Power on the 2-pin connector**, CAN **data (H/L) on the 4-pin**, 4-pin power
  pins left **open**, **VLink NOT jumpered**.
- Equivalent alternative (electrically identical for current): feed the board's
  12 V through the 4-pin power pins + closed VLink. Preferred form above uses the
  2-pin (one fewer jumper; cleaner if a separate board supply is ever added).
- **No galvanic isolation** in this single-source build: pilot, ESP32, and the
  SeaTalk reference all share one 12 V / ground. Accepted trade (same as the old
  rig). The opto block still provides correct open-collector drive + transient
  protection on the SeaTalk line.

## Final pin map (firmware updated in `src/Lib/SeaTalk.h`)

| Function | GPIO | SH-ESP32 pad (Rev 2.1.0) | Note |
|---|---|---|---|
| `RX_IN`  | **35** | **ISO_IN**  (OPTO_IN)  | input-only pin, correct for RX |
| `TX_OUT` | **33** | **ISO_OUT** (OPTO_OUT) | unchanged, already matched board |
| `LED_PIN`| **2**  | on-board blue LED      | unchanged |

`_mySerial.begin(4800, SWSERIAL_8S1, RX_IN, TX_OUT, false, 95)` — **`invert=false`**
for the opto block. Both stages are non-inverting: SeaTalk idle 12 V → opto conducts
→ GPIO35 high = UART mark, and GPIO33 high (UART idle) → Q901 off → DATA stays high.
(The current NPN build inverts on both stages and uses `invert=true`.)

## Proto-area BOM (only these are hand-soldered)

| Ref | Value | Role |
|---|---|---|
| R_pull | **10 kΩ** | SeaTalk DATA bus pull-up to on-board +12 V (replicates original R3) |
| R_in   | **5.6 kΩ** (was 2.2 kΩ) | series current-limit, DATA → **ISO_IN** pad. With the on-board R905 (2.2 kΩ) this gives ~1.4 mA LED current at 12 V. The original 2.2 kΩ drew ~2.5 mA and held DATA near 4.4 V. |
| ~~D_out~~ | **none** (was 1N5817, fitted backwards) | **ISO_OUT connects directly to DATA.** ISO_OUT is already an open-collector NPN (Q901). If a diode is ever wanted here, the band (cathode) must face **ISO_OUT**, but it adds ~0.3 V to the driven low and stops R904 helping the pull-up. |

Dropped vs. original discrete board (the on-board opto block contains its own
output transistor, Q901, and pull-up, R904 3.9 kΩ): U2 7805, C1, C2 (→ on-board SMPS);
U3 6N137, R5, C3, R1 (→ opto IN); Q2, R2, R7 (→ opto OUT); Q1, R4, R6 (→ opto IN
replaces RX buffer). Original R3 (10 kΩ bus pull-up) is retained as **R_pull**.

## ASCII schematic

```
        On-board +12 V (bus 12 V; no "Protected Voltage" pad on this rev)
        |                                   |
        |                                   +---------------------+
       [R_pull 10k]                         |                     |
        |                              V_ISO pad            (V_ISO powers
        |                              (= "Vext")            the ISO_OUT driver)
        |                                   |
  J1.2  o------+-------------------+---------(---------------------+
  SeaTalk DATA |                   |         |                     |
              [R_in 5.6k]          |      +--+--------------------+|
               |                   |      |  SH-ESP32 OPTO BLOCK  ||
        ISO_IN pad o--------+      |      |  (Rev 2.1.0)          ||
                            |      |      | ISO_IN  = GPIO35 (RX) ||
                            |      |      | ISO_OUT = GPIO33 (TX) ||
                            |      |      +-----------------------+|
                            |      |                               |
                            |      +---------- ISO_OUT pad --------+
                            |        direct wire, no diode
                            |        (ISO_OUT = Q901 collector, 3.9k
                            |         pull-up R904 to V_ISO; pulls
                            |         DATA low when GPIO33 = LOW)
                            |
   J1.1 +12V ---> on-board +12 V (bus side)
   J1.3 GND  ---> board GND  === ISO_GND pad === SeaTalk signal ground
```

Signal flow:
- **RX:** DATA pulses low → ISO_IN opto LED turns off → OPTO_IN/GPIO35 goes low
  (it follows DATA, non-inverting) → SoftwareSerial with `invert=false` decodes 4800-8S1.
- **TX:** GPIO33 LOW → on-board opto off → R903 turns Q901 on → ISO_OUT pulls DATA
  to ~0.2 V. GPIO33 HIGH → Q901 off → R_pull and R904 return DATA high
  (non-inverting).
  Firmware reads each TX byte back on RX for collision detection — that loopback
  works because TX and RX share the one DATA node.

## Pad-by-pad wiring table (keyed to the B–T / 1–7 proto grid)

Named board nodes referenced: **+12V** = on-board bus 12 V, **GND** = any board
ground pad, and the Rev 2.1.0 opto pads **ISO_IN / ISO_OUT / V_ISO / ISO_GND**
(bottom-side silkscreen). Grid cells are a suggested compact placement — only the
net connectivity matters.

| Net | From | To | Via part | Suggested proto cell(s) |
|---|---|---|---|---|
| +12V | on-board +12 V | V_ISO pad | wire | route along row 1 |
| +12V | on-board +12 V | R_pull top | wire | B1 |
| DATA | J1 pin2 (Data) | R_pull bottom | wire | B2–C2 |
| DATA | R_pull bottom | R_in "hot" end | wire | C2–D2 |
| DATA | R_in "hot" end | ISO_OUT pad | wire (no diode) | D2 → ISO_OUT |
| DATA_IN | R_in other end | ISO_IN pad | wire | D3 → ISO_IN |
| GND | J1 pin3 (GND) | board GND pad | wire | — |
| GND | board GND pad | ISO_GND pad | wire | — |

Placement sketch (proto grid, `.` = unused hole):

```
col:  B    C    D    E    F ...
row1 [+12]-........................  (feed +12V to R_pull top & V_ISO)
row2 [Rpl]-[DATA]-[Rin]-..........  (R_pull | R_in along the DATA node)
row3  Rin->ISO_IN pad ... DATA wire->ISO_OUT pad (no diode)
```

## Safety / bring-up checklist

1. Power the SH-ESP32 from the 12 V feed first; confirm the 3.3 V rail. Do NOT
   connect J1 Data yet.
2. Bench-test RX against a known SeaTalk source (or the ST2000+) before enabling
   TX. Confirm `invert=false` yields correctly-framed bytes at GPIO35, and that
   DATA idles at 9 V or more with the RX leg connected.
3. Verify TX drives DATA **low only** (scope the line): idle must sit high
   (~9 V or more with the RX leg loading it), pulses go to near 0 V. With GPIO33
   held LOW, DATA should read near 0 V; with GPIO33 HIGH, DATA should be high.
   A stuck-low line will jam the ST2000+ and any other SeaTalk instrument.
4. Confirm the ST2000+ motor current (0.5–1.5 A) returns to the bus on its own
   drop and does NOT flow through the SH-ESP32 (check the board's input is not
   warm / polyfuse not tripping while the pilot steers under load).
5. Only then connect to the live SeaTalk bus with the tiller pilot.
```
