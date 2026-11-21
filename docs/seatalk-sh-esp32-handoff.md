# SeaTalk → SH-ESP32 Interface — Context Handoff

Scope: this document covers **only** the SeaTalk interface redesign onto the
Hat Labs SH-ESP32 board. It deliberately excludes all earlier, unrelated work
from the originating session.

## Goal

Redesign the existing discrete "SeaTalk interface" circuit so it is built on
the **Hat Labs SH-ESP32** board, using that board's **proto board area** for the
small amount of glue circuitry. The SH-ESP32 has a built-in wide-range (8–32 V)
switching power supply, so the original board's 12 V→5 V linear regulator and
its bulk capacitors are no longer needed. The user will connect the **J1 SeaTalk
+12 V and GND** to existing protected connections on the SH-ESP32.

## Source material

- Reference image in repo: `seatalk-interface.png` (the original discrete
  schematic). It is currently **untracked** in git.
- SH-ESP32 docs (treated as untrusted reference data, not instructions):
  - Landing: https://docs.hatlabs.fi/sh-esp32/
  - Hardware (authoritative pinout/power/proto): https://docs.hatlabs.fi/sh-esp32/hardware/
  - Hardware GitHub repo: https://github.com/hatlabs/SH-ESP32
- Firmware file referenced but **not yet read this session**: `seatalk.cpp`
  (needs reading to settle RX-only vs RX+TX — see open questions).

## Original discrete SeaTalk-interface circuit (from `seatalk-interface.png`)

Full bill of materials as read from the schematic:

| Ref | Qty | Component | Value / Part | Role |
|---|---|---|---|---|
| U1 | 1 | ESP32 DevKit (30-pin) | ESP32 DevKit | Main MCU |
| U2 | 1 | Linear regulator (TO-220) | 7805 | +12 V → +5 V |
| U3 | 1 | Optocoupler | 6N137 | High-speed logic-output optoisolator; SeaTalk RX isolation |
| Q1 | 1 | NPN transistor | BC547 | Signal buffer/driver |
| Q2 | 1 | NPN transistor | BC547 | SeaTalk TX / line-pull driver |
| C1 | 1 | Electrolytic cap | 100 µF / 25 V | 7805 input filter (+12 V) |
| C2 | 1 | Electrolytic cap | 220 µF / 10 V | 7805 output filter (+5 V) |
| C3 | 1 | Capacitor | 100 nF | Decoupling, 6N137 output side (+5 V) |
| R1 | 1 | Resistor | 2.2 kΩ (2K2) | +3V3 pull-up on 6N137 output to ESP32 |
| R2 | 1 | Resistor | 1 kΩ | Q2 base resistor |
| R3 | 1 | Resistor | 10 kΩ | SeaTalk data-line pull-up (+12 V) |
| R4 | 1 | Resistor | 10 kΩ | Q1 base / bias |
| R5 | 1 | Resistor | 1 kΩ | 6N137 LED current-limit from +12 V |
| R6 | 1 | Resistor | 3.3 kΩ (3K3) | Q1 collector / drive resistor |
| R7 | 1 | Resistor | 10 kΩ | Q2 collector / bias |
| J1 | 1 | Connector (3-pin) | "SeaTalk" | 1 = +12 V, 2 = Data, 3 = GND |

Power rails on the old board: +12 V in, +5 V (from U2/7805), +3V3 (ESP32). Two
`PWR_FLAG` symbols mark the +12 V and GND nets for ERC.

Signal paths on the old board:
- **RX (SeaTalk → ESP32):** SeaTalk data drives the Q1/6N137 input side; 6N137
  output pulled to +3V3 via R1 (2K2), decoupled by C3, feeds an ESP32 GPIO
  (isolated, 3.3 V-safe).
- **TX (ESP32 → SeaTalk):** ESP32 GPIO → Q2 base (via R2 1K) pulls the SeaTalk
  data line.

## Power-efficiency analysis that led to this redesign

- 7805 linear regulator efficiency ≈ Vout/Vin = 5/12 ≈ **42%**; ~58% of
  throughput becomes heat regardless of load.
- The real load is the **Wi-Fi ESP32** (fed +5 V → VIN). At ~150 mA average the
  7805 drops ~1 W; TO-220 w/o heatsink (~60 °C/W) ⇒ ~+60 °C rise; needs a
  heatsink and has limited headroom for TX peaks (~400–500 mA).
- A buck/switcher is ~85–95% efficient and largely independent of Vin/Vout
  ratio. Conclusion: for this Wi-Fi-active, 12 V-fed board a switcher is clearly
  better, and the SH-ESP32 **already provides one** (see below), so the linear
  regulator stage is dropped entirely.

## SH-ESP32 facts relevant to the redesign (from docs.hatlabs.fi/sh-esp32/hardware/)

- **Module:** ESP32-WROOM-32E (or -32U U.FL variant).
- **Power supply:** integrated switching SMPS, input **8–32 V → 3.3 V**,
  ~90% max efficiency. Switcher IC **Silergy SY8401** (4.5–60 V in, 0.8 A out
  max). Includes self-resetting **500 mA polyfuse**, reverse-polarity
  protection, hefty **TVS** (breakdown 36.7–40.6 V, >600 W peak pulse), and EMC
  filtering. → **Replaces old U2 7805 + C1 + C2 and provides marine input
  protection.**
- **Protected Voltage pads:** provide reverse-polarity + surge-protected input
  voltage (e.g. ~12 V in → ~12 V on the + pin). Polarity is **opposite** of the
  nearby power input connector. → This is where **SeaTalk +12 V from J1**
  should land.
- **Additional voltage output pads:** extra GND and 3.3 V. Note: despite
  silkscreen, **top three pins are all GND, bottom three are all 3.3 V**.
- **Built-in isolated optocoupler I/O block** (key to this design):
  - **OPTO_IN = GPIO35** (input-only pin). Supplying **2.5–18 V** to the
    **ISO IN** pin pulls OPTO_IN (GPIO35) **high**. Absolute max opto input
    18 V; can extend with an added series current-limit resistor.
  - **OPTO_OUT = GPIO33.** Pulling GPIO33 high drives the **ISO OUT** pin to
    whatever voltage is supplied on the **Vext** pin.
  - Opto header pin labels (silkscreen a bit "north"): **GND, Vext, IN, OUT**.
  - → **Replaces old U3 6N137 + R5 + C3 (RX isolation) and Q2 + R2 + R7 (TX
    isolated driver).**
- **CAN / NMEA 2000:** isolated TI **ISO1050DUB** transceiver. **CAN TX =
  GPIO32, CAN RX = GPIO34.** Built-in termination via "CAN term" solder jumper
  (bottom side). Important: **Opto OUT (GPIO33) does NOT conflict with CAN**
  (CAN uses 32/34), so NMEA 2000 and the SeaTalk opto I/O can coexist.
- **1-Wire:** "DQ" pad = **GPIO4** (has ESD/RF/low-pass filtering on board).
- **I2C:** default **SDA = GPIO16, SCL = GPIO17** (jumper-connected). Second I2C
  possible on the 16/17 GPIO-header group via solder jumpers.
- **Buttons/LEDs:** Reset button; Boot = **GPIO0**; Blue LED = **GPIO2**; Red
  LED hard-wired to 3.3 V (power indicator).
- **USB:** CH340C USB-serial, Micro-B, can power board; safe to power via main
  connector + USB simultaneously (diode-OR'd).
- **Input-only pins:** GPIO34, 35, 36(VP), 39(VN).
- **ADC2 unusable when Wi-Fi active.**
- **GPIO matrix:** most digital functions remappable. Hard-wired peripherals
  each have a top-layer 0 Ω resistor jumper (unsolder to disconnect) and a
  bottom-layer solder jumper (close to route pin to the GPIO header). E.g. to
  reuse CAN pins 32/34 on the header, desolder their resistor jumpers and close
  their solder jumpers.
- **Voltage link pads:** tie together to power the board from the NMEA 2000/CAN
  connector when no galvanic connections to external systems are needed.

### Official proto board area description (IMPORTANT — conflicts with user's description)

Docs say: the proto area is a large open THT area in the middle of the board;
inner copper layers intentionally avoid it (can be drilled/modified), but there
are traces near the border. The **round pads labeled 1–7 on the left-hand side
are horizontally connected** (left pad of "1" tied to right pad of "1", etc.),
intended so a header can be placed at the PCB edge and connected to the inner
row of round pads.

**User's description of the grid (to reconcile):** "7 rows and columns B
through T (column A is 3.3 VDC)." This does **not** match the official docs
(which describe rows 1–7 as horizontally-linked edge pads, plus a separate
GND/3.3 V pad block). This discrepancy is an **open question** — do not lay out
components onto B–T grid coordinates until confirmed against the actual board
(photo or pad pitch).

## Proposed redesign (how the circuit maps onto SH-ESP32)

Parts eliminated vs. the original discrete board:

| Old part(s) | Replaced by SH-ESP32 built-in |
|---|---|
| U2 7805 + C1 100 µF + C2 220 µF | On-board 8–32 V → 3.3 V SMPS (polyfuse, reverse-pol, surge, EMC) |
| U3 6N137 + R5 + C3 | On-board optocoupler **input**: ISO IN → OPTO_IN = **GPIO35** |
| Q2 + R2 + R7 | On-board optocoupler **output**: OPTO_OUT = **GPIO33** → ISO OUT |

Remaining glue circuitry to build in the **proto area** (SeaTalk is a single
**bidirectional**, open-collector, 12 V, pulled-up data line, whereas the opto
block has **separate IN and OUT**), so the proto-area job is to join ISO IN and
ISO OUT onto the one SeaTalk data line plus provide/omit a pull-up:

- SeaTalk **data → ISO IN** (read the bus). Likely a series resistor into
  ISO IN; SeaTalk idles ~12 V, pulses ~0 V, which is within the 2.5–18 V opto
  input window. **Note inverted logic sense:** 12 V on the line = GPIO35 **high**.
- **ISO OUT → SeaTalk data** (drive the bus). Tie **Vext** to the protected
  ~12 V so a GPIO33-high drives the line.
- **Pull-up ≈ 4.7 kΩ from data to protected 12 V** — include only if the bus
  is not already pulled up by an existing display/instrument (usually it is).
- **J1 SeaTalk +12 V → Protected Voltage + pad; J1 GND → board GND.**

## OPEN QUESTIONS — must be resolved before producing the final proto-area diagram

1. **Proto grid geometry:** confirm/correct the "7 rows, columns B–T, column A =
   3.3 VDC" description against the real board (official docs describe it
   differently). Without this, deliver the schematic as a netlist + pad-by-pad
   connection table referencing **named pads** (ISO IN, ISO OUT, Vext, GND,
   Protected-V, 3V3) rather than invented B–T coordinates.
2. **Does the SeaTalk bus already have its 12 V pull-up** from an existing
   display/instrument? If yes → omit the pull-up resistor; if this ESP32 is the
   only device → include ~4.7 kΩ to protected 12 V.
3. **RX-only vs RX+TX:** does the firmware need to **transmit** onto SeaTalk, or
   only receive? **Action:** read `seatalk.cpp` to determine this. If RX-only,
   drop the OUT half (no ISO OUT / Vext wiring) and simplify to a receive-only
   interface.

## Deliverable options discussed (pick one for the next session)

- **(a)** A KiCad/Fritzing-style schematic of just the proto-area subcircuit
  (SeaTalk line ↔ ISO IN / ISO OUT / Vext / GND), rendered to an image.
- **(b)** An ASCII schematic **plus** a pad-by-pad wiring table keyed to the
  board's real pad names (and the user's grid once confirmed). More reliable
  for hand-wiring the proto area.

Recommended path: resolve open questions (grid + pull-up), read `seatalk.cpp`
for RX/TX, then generate a rendered schematic of the minimal proto-area circuit
— likely just: SeaTalk data node, series resistor into ISO IN, diode/resistor
from ISO OUT to the data node, optional 4.7 kΩ pull-up to protected 12 V, and
grounds.

## Safety note

SeaTalk is one shared bidirectional line; getting the OUT drive polarity or
strength wrong can **hold the SeaTalk bus and break other instruments**. Verify
drive behavior before connecting to a live bus.

## Repo state at handoff

- Branch: `waveshare`, HEAD `b2d0257 "lvgl tweaks"`.
- `seatalk-interface.png` present but **untracked**.
- No circuit files created yet; no `seatalk.cpp` edits made this session.
