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

/// @brief Method to run messages
void SeaTalk::processMessages()
{
    txHoldLoop();
    checkBus();
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

/// @brief Checks the seatalk bus for messages
/// @return if valid
int SeaTalk::checkBus()
{
    static std::vector<uint8_t> message;

    while (_mySerial.available())
    {
        uint8_t inByte = _mySerial.read();
        bool parity = _mySerial.readParity();
        optoRxEdges++;

        if (parity) // Command byte received
        {
            message.clear();
            message.push_back(inByte);
        }
        else if (!message.empty()) // Data byte received
        {
            message.push_back(inByte);

            // Process messages
            // Apparent Wind Angle
            if (message[0] == 0x10 && message.size() == 4)
            {
                stRxPackets++;
                double apparentWindAngle = ((message[2] << 8) | (message[3])) / 2.0;
                if (seatalkDebugRx) {
                    snprintf(logbuf, LOGBUF_SIZE, "ST RX: Apparent Wind Angle: %.1f degrees", apparentWindAngle);
                    log::toAll(logbuf);
                }
                _signalManager->UpdateApparentWindAngle(apparentWindAngle);
            }
            // Apparent Wind Speed
            else if (message[0] == 0x11 && message.size() == 4)
            {
                stRxPackets++;
                double apparentWindSpeed = ((message[2] & 0x7f) + (message[3] & 0x0f) / 10.0);
                if (seatalkDebugRx) {
                    snprintf(logbuf, LOGBUF_SIZE, "ST RX: Apparent Wind Speed: %.1f knots", apparentWindSpeed);
                    log::toAll(logbuf);
                }
                _signalManager->UpdateApparentWindSpeed(apparentWindSpeed);
            }
            // Speed Through Water
            else if (message[0] == 0x20 && message.size() == 4)
            {
                stRxPackets++;
                double speedThroughWater = ((message[3] << 8) | message[2]) / 10;
                if (seatalkDebugRx) {
                    snprintf(logbuf, LOGBUF_SIZE, "ST RX: Speed Through Water: %.1f knots", speedThroughWater);
                    log::toAll(logbuf);
                }
                _signalManager->UpdateSpeedThroughWater(speedThroughWater);
            }
            // Speed Over Ground
            else if (message[0] == 0x52 && message.size() == 4)
            {
                stRxPackets++;
                double speedOverGround = ((message[3] << 8) | message[2]) / 10;
                if (seatalkDebugRx) {
                    snprintf(logbuf, LOGBUF_SIZE, "ST RX: Speed Over Ground: %.1f knots", speedOverGround);
                    log::toAll(logbuf);
                }
                _signalManager->UpdateSpeedOverGround(speedOverGround);
            }
            // Course Over Ground
            else if (message[0] == 0x53 && message.size() == 4)
            {
                stRxPackets++;
                uint8_t u = (message[1] & 0xf0) >> 4;
                uint8_t vw = message[2];
                double courseOverGround = (u & 0x3) * 90 + (vw & 0x3F) * 2 + ((u & 0xC) >> 2) / 2;
                if (seatalkDebugRx) {
                    snprintf(logbuf, LOGBUF_SIZE, "ST RX: Course Over Ground: %.1f degrees", courseOverGround);
                    log::toAll(logbuf);
                }
                _signalManager->UpdateCourseOverGround(courseOverGround);
            }
            // Depth Below Transducer
            else if (message[0] == 0x00 && message.size() == 5)
            {
                stRxPackets++;
                double depthBelowTransducer = (((message[4] >> 8) | message[3]) / 10);
                if (seatalkDebugRx) {
                    snprintf(logbuf, LOGBUF_SIZE, "ST RX: Depth Below Transducer %.1f Meters", depthBelowTransducer);
                    log::toAll(logbuf);
                }
            }
            // Auto Pilot Data
            else if (message[0] == 0x84 && message.size() == 9)
            {
                stRxPackets++;
                uint8_t u = (message[1] & 0xf0) >> 4;
                uint8_t vw = message[2];

                AutoPilotData pilotData;
                pilotData.compassHeading = (u & 0x3) * 90 + (vw & 0x3F) * 2 + (u & 0xC ? (u & 0xC == 0xC ? 2 : 1) : 0);
                if (pilotData.compassHeading >= 360.0) pilotData.compassHeading -= 360.0;
                if (seatalkDebugRx) {
                    snprintf(logbuf, LOGBUF_SIZE, "ST RX: Auto Compass heading %.1f Degrees", pilotData.compassHeading);
                    log::toAll(logbuf);
                }
                _signalManager->UpdateCompassHeading(pilotData.compassHeading);

                // Transmit heading on N2K bus if enabled
#ifdef N2K
                n2kSendHeading(pilotData.compassHeading);
#endif

                uint8_t dir = (u & 0x80) >> 3;
                if (dir == 1)
                    pilotData.turningDirection = TurningDirection::port;
                else
                    pilotData.turningDirection = TurningDirection::starboard;

                uint8_t v = (message[2] & 0xc0) >> 6;
                uint8_t xy = message[3];
                pilotData.autoPilotCourse = (v * 90) + (xy / 2);
                if (seatalkDebugRx) {
                    snprintf(logbuf, LOGBUF_SIZE, "ST RX: Auto Pilot Course= %.1f Degrees", pilotData.autoPilotCourse);
                    log::toAll(logbuf);
                }

                uint8_t z = message[4];
                if (z & 0x2 == 0)
                    pilotData.autoPilotMode = standby;
                else if (z & 0x2 == 2)
                    pilotData.autoPilotMode = autom;
                else if (z & 0x2 == 4)
                    pilotData.autoPilotMode = vane;
                else if (z & 0x8 == 8)
                    pilotData.autoPilotMode = track;
            }
            // Nauti-Control Echo Command
            else if (message[0] == 0xAC)
            {
                stRxPackets++;
                if (message[1] == 0x0)
                {
                    uint8_t testNumber = message[3];
                    uint8_t responsemessage[] = {0xAC, 0x01, 0x00, testNumber};
                    send2ST(responsemessage, 4);
                }
            }
        }
    }

    return -1;
}

/// @brief Send Command To Seatalk Bus
/// @param cmd Command to send
void SeaTalk::sendCommand(commands cmd)
{
    snprintf(logbuf, LOGBUF_SIZE, "%d", cmd);
    log::toAll(logbuf);
    if (cmd == minus_1) {
        send2ST(ST_Minus_1, 4);
    } else if (cmd == plus_1) {
        send2ST(ST_Plus_1, 4);
    } else if (cmd == minus_10) {
        send2ST(ST_Minus_10, 4);
    } else if (cmd == plus_10) {
        send2ST(ST_Plus_10, 4);
    } else if (cmd == auto_but) {
        send2ST(ST_Auto, 4);
    } else if (cmd == standby_but) {
        send2ST(ST_Standby, 4);
    } else if (cmd == start_timer) {
        send2ST(ST_TimerStart, 5);
    } else if (cmd == wind_mode) {
        send2ST(ST_Wind, 4);
    } else if (cmd == tack_port) {
        send2ST(ST_Minus_1, 4);
        send2ST(ST_Minus_10, 4);
    } else if (cmd == tack_starboard) {
        send2ST(ST_Plus_1, 4);
        send2ST(ST_Plus_10, 4);
    }
}

/// @brief Send compass heading to Seatalk
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

/// @brief Send apparent wind angle to SeaTalk bus
/// @param angle Wind angle in degrees
void SeaTalk::sendApparentWindAngle(double angle)
{
    uint16_t encodedAngle = (uint16_t)(angle * 2.0);
    uint8_t message[] = {0x10, 0x01, (uint8_t)(encodedAngle >> 8), (uint8_t)(encodedAngle & 0xFF)};
    send2ST(message, 4);
}

/// @brief Send apparent wind speed to SeaTalk bus
/// @param speedKnots Wind speed in knots
void SeaTalk::sendApparentWindSpeed(double speedKnots)
{
    uint8_t speedInt = (uint8_t)speedKnots;
    uint8_t speedDec = (uint8_t)((speedKnots - speedInt) * 10);
    uint8_t message[] = {0x11, 0x01, (uint8_t)(speedInt & 0x7F), (uint8_t)(speedDec & 0x0F)};
    send2ST(message, 4);
}

/// @brief Send to seatalk Bus (blind send, no echo check)
/// Transmits all bytes, then checks for unexpected bus traffic that may indicate
/// another device was transmitting simultaneously.
/// @param cmd cmd
/// @param bytes length
/// @return did send
bool SeaTalk::send2ST(const uint8_t cmd[], int bytes)
{
    int attempt = 0;
    const int maxRetries = 5;

    if (_txHolding) {
        log::toAll("ST TX skipped: txhold in progress");
        return false;
    }

    if (seatalkDebugTx) {
        snprintf(logbuf, LOGBUF_SIZE, "ST TX: cmd=0x%02X len=%d [%02X %02X %02X %02X]",
            cmd[0], bytes, cmd[0], cmd[1], bytes > 2 ? cmd[2] : 0, bytes > 3 ? cmd[3] : 0);
        log::toAll(logbuf);
    }

    while (attempt < maxRetries)
    {
        checkClearToWrite();
        digitalWrite(LED_PIN, HIGH);

        // Transmit all bytes back to back. SoftwareSerial write() blocks for the
        // whole byte (~2.3 ms at 4800 baud), so no extra delay: a gap longer than
        // 10 bit times (~2.1 ms) tells other devices the bus is free, and they may
        // start talking in the middle of this datagram.
        for (int i = 0; i < bytes; i++)
        {
            (i == 0) ? _mySerial.write(cmd[i], SWSERIAL_PARITY_MARK) : _mySerial.write(cmd[i], SWSERIAL_PARITY_SPACE);
        }

        // Flush any echoed bytes from RX buffer (if TX is looped back)
        delay(5);
        while (_mySerial.available()) { _mySerial.read(); optoRxEdges++; }

        // Assume success (no echo verification possible on this hardware)
        digitalWrite(LED_PIN, LOW);
        stTxPackets++;
        if (seatalkDebugTx) {
            snprintf(logbuf, LOGBUF_SIZE, "ST TX OK: cmd=0x%02X after %d attempt(s)", cmd[0], attempt + 1);
            log::toAll(logbuf);
        }
        delay(50);
        return true;
    }

    stTxFails++;
    if (seatalkDebugTx) {
        snprintf(logbuf, LOGBUF_SIZE, "Send Failed: cmd=0x%02X after %d attempts (total fails=%lu)",
            cmd[0], maxRetries, stTxFails);
        log::toAll(logbuf);
    }
    digitalWrite(LED_PIN, LOW);
    return false;
}

/// @brief Checks bus clear to write by waiting for a silence gap
/// Waits until no data is received for 12ms, indicating end of a bus message
void SeaTalk::checkClearToWrite()
{
    unsigned long lastActivity = millis();
    unsigned long startWait = lastActivity;
    int bytesSkipped = 0;

    while (millis() - lastActivity < 12)
    {
        if (_mySerial.available())
        {
            _mySerial.read();
            optoRxEdges++;
            lastActivity = millis();
            bytesSkipped++;
        }
    }

    unsigned long waitTime = millis() - startWait;
    if ((bytesSkipped > 0 || waitTime > 20) && seatalkDebugTx) {
        snprintf(logbuf, LOGBUF_SIZE, "CTS: waited %lums, skipped %d bytes", waitTime, bytesSkipped);
        log::toAll(logbuf);
    }
}
