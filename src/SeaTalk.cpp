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
}

/// @brief Method to run messages
void SeaTalk::processMessages()
{
    checkBus();
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
        delay(100);
        send2ST(ST_Minus_1, 4);
    } else if (cmd == plus_1) {
        send2ST(ST_Plus_1, 4);
        delay(100);
        send2ST(ST_Plus_1, 4);
    } else if (cmd == minus_10) {
        send2ST(ST_Minus_10, 4);
        delay(100);
        send2ST(ST_Minus_10, 4);
    } else if (cmd == plus_10) {
        send2ST(ST_Plus_10, 4);
        delay(100);
        send2ST(ST_Plus_10, 4);
    } else if (cmd == auto_but) {
        send2ST(ST_Auto, 4);
        delay(100);
        send2ST(ST_Auto, 4);
    } else if (cmd == standby_but) {
        send2ST(ST_Standby, 4);
        delay(100);
        send2ST(ST_Standby, 4);
    } else if (cmd == start_timer) {
        send2ST(ST_TimerStart, 5);
        delay(100);
        send2ST(ST_TimerStart, 5);
    } else if (cmd == wind_mode) {
        send2ST(ST_Wind, 4);
        delay(100);
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

    if (seatalkDebugTx) {
        snprintf(logbuf, LOGBUF_SIZE, "ST TX: cmd=0x%02X len=%d", cmd[0], bytes);
        log::toAll(logbuf);
    }

    while (attempt < maxRetries)
    {
        checkClearToWrite();
        digitalWrite(LED_PIN, HIGH);

        // Transmit all bytes blind
        for (int i = 0; i < bytes; i++)
        {
            (i == 0) ? _mySerial.write(cmd[i], SWSERIAL_PARITY_MARK) : _mySerial.write(cmd[i], SWSERIAL_PARITY_SPACE);
            delay(1);
        }

        // Wait for bus to settle after our transmission
        delay(3);

        // Check if there's unexpected data on the bus (another device was talking)
        int strayBytes = 0;
        while (_mySerial.available())
        {
            _mySerial.read();
            strayBytes++;
        }

        if (strayBytes == 0)
        {
            // Clean send — no bus conflict detected
            digitalWrite(LED_PIN, LOW);
            stTxPackets++;
            if (seatalkDebugTx) {
                snprintf(logbuf, LOGBUF_SIZE, "ST TX OK: cmd=0x%02X after %d attempt(s)", cmd[0], attempt + 1);
                log::toAll(logbuf);
            }
            delay(50);
            return true;
        }

        // Stray bytes detected — possible collision
        attempt++;
        int backoff = random(50, 200);
        if (seatalkDebugTx) {
            snprintf(logbuf, LOGBUF_SIZE, "TX conflict: cmd=0x%02X stray=%d attempt=%d backoff=%dms",
                cmd[0], strayBytes, attempt, backoff);
            log::toAll(logbuf);
        }
        delay(backoff);
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
