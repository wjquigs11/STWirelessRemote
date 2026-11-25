#include "SeaTalk.h"
#include "logto.h"

// Forward declaration for N2K heading transmit
#ifdef N2K
extern void n2kSendHeading(double headingDeg);
#endif

// Global packet counters
unsigned long stRxPackets = 0;
unsigned long stTxPackets = 0;
unsigned long stTxFails = 0;

// Debug flags
bool seatalkDebugRx = true;   // debug for received messages (default true)
bool seatalkDebugTx = false;  // debug for transmitted messages (default false)

/// @brief Seatalk class constructor
/// @param signalManager Signal manager to send messages to other systems
SeaTalk::SeaTalk(SignalManager *signalManager)
{
    _mySerial.begin(4800, SWSERIAL_8S1, RX_IN, TX_OUT, true, 95);
    pinMode(LED_PIN, OUTPUT);
    _signalManager = signalManager;
    logOptoPins();
}

/// @brief Log the SeaTalk opto GPIO assignments and their current digital states.
///        RX_IN and TX_OUT are driven by SoftwareSerial; digitalRead() gives a
///        snapshot of the pin level. With the inverting NPN interface, an idle bus
///        (DATA at 12 V) reads RX_IN=0 and TX_OUT=0.
void SeaTalk::logOptoPins()
{
    int rxVal = digitalRead(RX_IN);
    int txVal = digitalRead(TX_OUT);
    snprintf(logbuf, LOGBUF_SIZE,
             "ST GPIO: RX_IN=GPIO%d val=%d, TX_OUT=GPIO%d val=%d, LED_PIN=GPIO%d",
             RX_IN, rxVal, TX_OUT, txVal, LED_PIN);
    log::toAll(logbuf);
}

// ── Bus activity monitoring (tog optolog, /optolog) ────────────────────────
// RX: bytes decoded by SoftwareSerial, counted where they are read. There must be
// NO extra interrupt on RX_IN: an ESP32 pin has one ISR, and attaching one there
// replaces SoftwareSerial's receive ISR (and detaching it kills RX until reboot).
// TX: edges on TX_OUT via a GPIO interrupt. Safe: SoftwareSerial uses no ISR on TX.
// Own transmissions that reach the bus come back as RX bytes (echo).
static volatile unsigned long optoRxEdges = 0;   // RX bytes (name kept for the counters below)
static volatile unsigned long optoTxEdges = 0;   // TX edges
// Running totals since counters were last read by getOptoChanges().
static volatile unsigned long optoRxEdgesLast = 0;
static volatile unsigned long optoTxEdgesLast = 0;
static unsigned long optoLastReadMs = 0;
static bool optoLogEnabled = false;
static bool optoIsrAttached = false;

static void IRAM_ATTR optoTxIsr() { optoTxEdges++; }

/// @brief Enable/disable bus activity monitoring and the once-per-second log.
/// Attaches a CHANGE interrupt on TX_OUT only (never on RX_IN, see above).
void SeaTalk::setOptoLog(bool on)
{
    if (on && !optoIsrAttached) {
        // Snapshot current totals as the baseline so the first web query
        // reports changes since enabling.
        optoRxEdges = optoTxEdges = 0;
        optoRxEdgesLast = optoTxEdgesLast = 0;
        optoLastReadMs = millis();
        attachInterrupt(digitalPinToInterrupt(TX_OUT), optoTxIsr, CHANGE);
        optoIsrAttached = true;
    } else if (!on && optoIsrAttached) {
        detachInterrupt(digitalPinToInterrupt(TX_OUT));
        optoIsrAttached = false;
    }
    optoLogEnabled = on;
    snprintf(logbuf, LOGBUF_SIZE, "ST opto monitor: %s", on ? "ON" : "OFF");
    log::toAll(logbuf);
}

bool SeaTalk::getOptoLog() const
{
    return optoLogEnabled;
}

/// @brief Return edge counts since the previous call, then reset the window.
void SeaTalk::getOptoChanges(unsigned long &rxChanges, unsigned long &txChanges,
                             unsigned long &intervalMs)
{
    noInterrupts();
    unsigned long rxNow = optoRxEdges;
    unsigned long txNow = optoTxEdges;
    interrupts();

    rxChanges = rxNow - optoRxEdgesLast;
    txChanges = txNow - optoTxEdgesLast;
    optoRxEdgesLast = rxNow;
    optoTxEdgesLast = txNow;

    unsigned long nowMs = millis();
    intervalMs = nowMs - optoLastReadMs;
    optoLastReadMs = nowMs;
}

/// @brief Log accumulated opto edge counts once per second and reset counters.
/// Logging every single edge would flood the system at 4800 baud, so this
/// emits a one-line summary at most once per second while monitoring is on,
/// then zeroes the counters so each line reports changes in the last second.
void SeaTalk::optoLogLoop()
{
    if (!optoLogEnabled) return;

    static unsigned long lastLogMs = 0;
    unsigned long nowMs = millis();
    if (nowMs - lastLogMs < 1000) return;
    lastLogMs = nowMs;

    noInterrupts();
    unsigned long rxCount = optoRxEdges;
    unsigned long txCount = optoTxEdges;
    optoRxEdges = 0;
    optoTxEdges = 0;
    interrupts();
    // Keep the web query's baseline consistent after this reset.
    optoRxEdgesLast = 0;
    optoTxEdgesLast = 0;

    snprintf(logbuf, LOGBUF_SIZE,
             "ST bus/s: RX bytes(GPIO%d)=%lu TX edges(GPIO%d)=%lu",
             RX_IN, rxCount, TX_OUT, txCount);
    log::toAll(logbuf);
}

/// @brief Sample raw RX_IN level over a window and report the bus state.
/// RX_IN is Q2's collector, the only software view of the J1.2 DATA node.
/// Q2 inverts: DATA at 12 V (idle) -> Q2 on -> RX_IN LOW; DATA pulled low -> RX_IN HIGH.
/// A steady HIGH means DATA is being held low (jammed bus, or Q2/R_b2 not connected).
/// No change while this device transmits means the RX stage is not seeing DATA.
void SeaTalk::optoProbe(unsigned long durationMs)
{
    // SoftwareSerial keeps RX_IN as an input, so sampling with digitalRead is non-destructive.
    unsigned long start = millis();
    unsigned long samples = 0, rxHigh = 0, rxLow = 0;
    while (millis() - start < durationMs) {
        if (digitalRead(RX_IN)) rxHigh++;
        else                    rxLow++;
        samples++;
    }
    int txLevel = digitalRead(TX_OUT);
    const char *state = (rxHigh == 0) ? "idle (DATA at 12V)"
                      : (rxLow == 0)  ? "DATA held LOW!"
                                      : "toggling (traffic)";
    snprintf(logbuf, LOGBUF_SIZE,
             "ST probe (%lums): RX_IN(GPIO%d) %s high=%lu low=%lu of %lu; TX_OUT(GPIO%d) now=%d",
             durationMs, RX_IN, state, rxHigh, rxLow, samples, TX_OUT, txLevel);
    log::toAll(logbuf);
}

/// @brief Main-loop work. Mirrors AK-Homberger's loop(): periodic beep-off and
/// bridge ID, receive wind, end a running beep. (Keys arrive via sendCommand().)
void SeaTalk::processMessages()
{
    txHoldLoop();
    unsigned long now = millis();

    if (!_started) {                 // AK setup(): send bridge ID once at start
        _started = true;
        _beepTimer2 = _bridgeTimer = now;
        sendDatagram(ST_NMEA_BridgeID, sizeof(ST_NMEA_BridgeID));
    }
    if (now - _beepTimer2 > 3000) {  // additional beep off every 3 s, avoids a constant alarm
        sendDatagram(ST_BeepOff, sizeof(ST_BeepOff));
        _beepTimer2 = now;
    }
    if (now - _bridgeTimer > 10000) {   // bridge ID every 10 s
        sendDatagram(ST_NMEA_BridgeID, sizeof(ST_NMEA_BridgeID));
        _bridgeTimer = now;
    }
    checkWind();
    beepOff();
}

/// @brief Start a TX hold: drive TX_OUT HIGH so Q1 pulls DATA low, for metering.
/// SoftwareSerial (invert=true) idles TX_OUT LOW, so writing HIGH is the "active" state.
/// Released by txHoldLoop(); refuses to start while one is already running.
void SeaTalk::txHold(unsigned long durationMs)
{
    if (_txHolding) {
        log::toAll("ST txhold: already holding");
        return;
    }
    _txHoldMs = durationMs;
    _txHoldRx = -1;
    _txHoldStart = millis();
    _txHolding = true;
    digitalWrite(TX_OUT, HIGH);
    snprintf(logbuf, LOGBUF_SIZE,
             "ST txhold: TX_OUT(GPIO%d) HIGH for %lu ms. Measure now: GPIO%d ~3.3V, Q1 base ~0.7V, DATA <0.5V",
             TX_OUT, durationMs, TX_OUT);
    log::toAll(logbuf);
}

/// @brief Sample RX_IN partway through a hold, and release TX_OUT when time is up.
void SeaTalk::txHoldLoop()
{
    if (!_txHolding) return;
    unsigned long elapsed = millis() - _txHoldStart;
    // Sample once, after the line has settled. Q2 inverts: DATA low -> RX_IN HIGH.
    if (_txHoldRx < 0 && elapsed >= 200) {
        _txHoldRx = digitalRead(RX_IN);
    }
    if (elapsed < _txHoldMs) return;
    digitalWrite(TX_OUT, LOW);       // back to SoftwareSerial idle
    _txHolding = false;
    while (_mySerial.available()) { _mySerial.read(); optoRxEdges++; }   // discard bytes decoded from the held-low line
    snprintf(logbuf, LOGBUF_SIZE,
             "ST txhold: released. During hold RX_IN(GPIO%d)=%d -> %s",
             RX_IN, _txHoldRx,
             _txHoldRx == 1 ? "DATA went LOW: TX reaches the bus"
                            : "DATA stayed HIGH: TX does NOT reach the bus (check Q1, R_b1, GPIO26 wire)");
    log::toAll(logbuf);
}

// ════════════════════════════════════════════════════════════════════════════
// Port of AK-Homberger's ArduinoPilotMicro433WindNG.ino (Seatalk-Autopilot-Remote-
// Control). His sketch uses a 9-bit hardware Serial1 on a Pro Micro; here the 9th
// (command) bit is EspSoftwareSerial's parity bit, MARK = command byte.
// ════════════════════════════════════════════════════════════════════════════

#define ST_KEY_DELAY 300      // AK KEY_DELAY: 300 ms break between keys
#define ST_ECHO_WAIT_US 20000 // AK waits delay(3); diagnostic: wait up to 20 ms and log how long it took

/// @brief Equivalent of AK's 9-bit Serial1.read(): byte | 0x100 for a command byte.
int SeaTalk::read9()
{
    if (!_mySerial.available()) return -1;
    int b = _mySerial.read();
    bool cmdBit = _mySerial.readParity();
    optoRxEdges++;
    return b | (cmdBit ? 0x100 : 0);
}

/// @brief AK sendDatagram(): wait for silence, write each byte and check its echo,
/// stop at the first mismatch (collision), up to 5 tries with a random back-off.
/// Length comes from the attribute nibble, as in AK's code.
bool SeaTalk::sendDatagram(const uint8_t data[], int size)
{
    if (_txHolding) {
        log::toAll("ST TX skipped: txhold in progress");
        return false;
    }
    int bytes = (data[1] & 0x0f) + 3;   // message length is minimum 3, extra bytes in the attribute nibble
    if (bytes > size) {
        snprintf(logbuf, LOGBUF_SIZE, "ST TX: datagram 0x%02X says %d bytes but has %d, not sent",
                 data[0], bytes, size);
        log::toAll(logbuf);
        return false;
    }

    int j = 0;
    int i = 0;
    int want = 0, got = 0;                           // last compared byte, for the log
    while (j < 5) {                                  // CSMA/CD, 5 tries
        int drained = 0;
        char drainStr[40] = "";
        int dpos = 0;
        while (_mySerial.available()) {              // wait for silence on the bus
            int b = read9();
            if (drained < 6 && dpos < (int)sizeof(drainStr) - 5)
                dpos += snprintf(drainStr + dpos, sizeof(drainStr) - dpos, "%03X ", b);
            drained++;
            delay(3);
        }
        if (seatalkDebugTx && drained) {
            snprintf(logbuf, LOGBUF_SIZE, "ST TX diag: drained %d byte(s) before try %d: %s", drained, j + 1, drainStr);
            log::toAll(logbuf);
        }

        bool ok = true;
        digitalWrite(LED_PIN, HIGH);
        for (i = 0; (i < bytes) && ok; i++) {        // write and listen to detect collisions
            int outbyte = data[i] | (i == 0 ? 0x100 : 0);
            _mySerial.write(data[i], i == 0 ? SWSERIAL_PARITY_MARK : SWSERIAL_PARITY_SPACE);
            // AK: delay(3) then check. Our write() blocks for the whole byte, so a fixed
            // 3 ms would leave a gap; wait only until the echo has been decoded.
            unsigned long t0 = micros();
            while (!_mySerial.available() && micros() - t0 < ST_ECHO_WAIT_US) {}
            unsigned long echoUs = micros() - t0;
            bool ovf = _mySerial.overflow();
            int inbyte = read9();
            want = outbyte;
            got = inbyte;
            if (seatalkDebugTx) {
                snprintf(logbuf, LOGBUF_SIZE, "ST TX diag: byte %d sent %03X, echo %s after %lu us%s",
                         i, outbyte, inbyte < 0 ? "none" : "arrived", echoUs, ovf ? " (rx overflow flag set)" : "");
                log::toAll(logbuf);
            }
            if (inbyte < 0) ok = false;              // nothing received
            else if (inbyte != outbyte) ok = false;  // not what we sent: collision
        }
        digitalWrite(LED_PIN, LOW);

        if (ok) {
            stTxPackets++;
            if (seatalkDebugTx) {
                snprintf(logbuf, LOGBUF_SIZE, "ST TX OK: %02X %02X ... (%d bytes) try %d",
                         data[0], data[1], bytes, j + 1);
                log::toAll(logbuf);
            }
            return true;
        }
        j++;                                         // collision detected
        if (seatalkDebugTx) {
            // 9-bit values: 1xx = command bit set. "none" = no echo within 3 ms.
            char gotStr[8];
            if (got < 0) snprintf(gotStr, sizeof(gotStr), "none");
            else snprintf(gotStr, sizeof(gotStr), "%03X", got);
            snprintf(logbuf, LOGBUF_SIZE, "ST TX collision: %02X byte %d sent %03X got %s, try %d",
                     data[0], i - 1, want, gotStr, j);
            log::toAll(logbuf);
        }
        delay(random(2, 50));                        // random wait for next try
    }
    stTxFails++;
    snprintf(logbuf, LOGBUF_SIZE, "ST TX: Send Error (%02X %02X ...)", data[0], data[1]);
    log::toAll(logbuf);                              // AK: Display("Send Error")
    return false;
}

/// @brief Kept for existing callers (wind, compass, timer).
bool SeaTalk::send2ST(const uint8_t cmd[], int bytes)
{
    return sendDatagram(cmd, bytes);
}

/// @brief AK checkWind(): receive apparent wind speed (11 01 XX 0Y) from the bus.
/// Reads one byte per call and discards anything else, as in AK's sketch.
int SeaTalk::checkWind()
{
    int wind = -1;
    if (_mySerial.available()) {
        int inbyte = read9();
        if (inbyte == 0x111) {          // AWS SeaTalk command, see Thomas Knauf
            delay(3);
            inbyte = read9();
            if (inbyte == 0x01) {
                delay(3);
                int xx = read9();
                delay(3);
                int y = read9();
                wind = (xx & 0x7f) + (y / 10);   // wind speed
                if (wind < 100) {                // greater than 100 must be a receive error
                    stRxPackets++;
                    if (seatalkDebugRx) {
                        snprintf(logbuf, LOGBUF_SIZE, "ST RX: AWS %d kn", wind);
                        log::toAll(logbuf);
                    }
                    _signalManager->UpdateApparentWindSpeed(wind);
                }
            }
        }
    }
    return wind;
}

/// @brief AK BeepOn(): SeaTalk alarm on, so instruments beep when a key is sent.
void SeaTalk::beepOn()
{
    if (_beepStatus) return;            // already on
    sendDatagram(ST_BeepOn, sizeof(ST_BeepOn));
    _beepTime = millis();
    _beepStatus = true;
}

/// @brief AK BeepOff(): alarm off after BEEP_DURATION.
void SeaTalk::beepOff()
{
    if (_beepStatus && millis() - _beepTime > BEEP_DURATION) {
        sendDatagram(ST_BeepOff, sizeof(ST_BeepOff));
        _beepStatus = false;
    }
}

/// @brief Key handling from AK's loop(): at most one key per KEY_DELAY, send the
/// datagram, then beep.
void SeaTalk::sendCommand(commands cmd)
{
    if (millis() - _keyTime < ST_KEY_DELAY) {
        log::toAll("ST: key ignored (less than 300 ms after the last one)");
        return;
    }
    _keyTime = millis();

    switch (cmd) {
    case minus_1:     sendDatagram(ST_Minus_1, sizeof(ST_Minus_1)); break;
    case plus_1:      sendDatagram(ST_Plus_1, sizeof(ST_Plus_1)); break;
    case minus_10:    sendDatagram(ST_Minus_10, sizeof(ST_Minus_10)); break;
    case plus_10:     sendDatagram(ST_Plus_10, sizeof(ST_Plus_10)); break;
    case auto_but:    sendDatagram(ST_Auto, sizeof(ST_Auto)); break;
    case standby_but: sendDatagram(ST_Standby, sizeof(ST_Standby)); break;
    // not in AK's sketch:
    case start_timer: sendDatagram(ST_TimerStart, sizeof(ST_TimerStart)); break;
    case wind_mode:   sendDatagram(ST_Wind, sizeof(ST_Wind)); break;
    case tack_port:
        sendDatagram(ST_Minus_1, sizeof(ST_Minus_1));
        sendDatagram(ST_Minus_10, sizeof(ST_Minus_10));
        break;
    case tack_starboard:
        sendDatagram(ST_Plus_1, sizeof(ST_Plus_1));
        sendDatagram(ST_Plus_10, sizeof(ST_Plus_10));
        break;
    default: return;
    }
    beepOn();
}

/// @brief Send compass heading to SeaTalk (not in AK's sketch).
void SeaTalk::sendCompass(float heading)
{
    uint16_t headingTenths = heading * 10;
    uint8_t message[] = {
        0x9C,
        0x02,
        (uint8_t)(headingTenths & 0xFF),
        (uint8_t)(headingTenths >> 8)
    };
    send2ST(message, sizeof(message));
}

/// @brief Send apparent wind angle to SeaTalk bus (not in AK's sketch).
void SeaTalk::sendApparentWindAngle(double angle)
{
    uint16_t encodedAngle = (uint16_t)(angle * 2.0);
    uint8_t message[] = {0x10, 0x01, (uint8_t)(encodedAngle >> 8), (uint8_t)(encodedAngle & 0xFF)};
    send2ST(message, 4);
}

/// @brief Send apparent wind speed to SeaTalk bus (not in AK's sketch).
void SeaTalk::sendApparentWindSpeed(double speedKnots)
{
    uint8_t speedInt = (uint8_t)speedKnots;
    uint8_t speedDec = (uint8_t)((speedKnots - speedInt) * 10);
    uint8_t message[] = {0x11, 0x01, (uint8_t)(speedInt & 0x7F), (uint8_t)(speedDec & 0x0F)};
    send2ST(message, 4);
}
