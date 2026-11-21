#ifndef SEATALK_H
#define SEATALK_H
#define BEEP_DURATION 150
#define MAX_BUF_SIZE 20
// SH-ESP32 pin mapping (Rev. 2.1.0; opto pads silkscreened ISO_OUT/ISO_IN/V_ISO/ISO_GND):
//   RX_IN  = GPIO35 -> on-board isolated opto INPUT  (OPTO_IN / "ISO_IN" pad). Input-only pin.
//   TX_OUT = GPIO33 -> on-board isolated opto OUTPUT (OPTO_OUT / "ISO_OUT" pad).
//   LED_PIN= GPIO2  -> on-board blue LED.
#define RX_IN 35
#define TX_OUT 33
#define LED_PIN 2
#define METRES_TO_FEET 3.28084
#define METRES_TO_FATHONS 0.546807
#define FEET_TO_METERS 0.3048

#include <SoftwareSerial.h>
#include "Commands.h"
#include "Models/AutoPilotData.h"
#include "SignalManager.h"

// Debug flags (extern'd in SeaTalk.cpp)
extern bool seatalkDebugRx;   // debug for received messages (default true)
extern bool seatalkDebugTx;  // debug for transmitted messages (default false)

class SeaTalk
{

public:
    SeaTalk(SignalManager *signalManager);

    const uint8_t ST_Minus_1[4] = {0x86, 0x21, 0x05, 0xFA};
    const uint8_t ST_Minus_10[4] = {0x86, 0x21, 0x06, 0xF9};
    const uint8_t ST_Plus_1[4] = {0x86, 0x21, 0x07, 0xF8};
    const uint8_t ST_Plus_10[4] = {0x86, 0x21, 0x08, 0xF7};
    const uint8_t ST_Auto[4] = {0x86, 0x21, 0x01, 0xFE};
    const uint8_t ST_Standby[4] = {0x86, 0x21, 0x02, 0xFD};
    const uint8_t ST_Wind[4] = {0x86, 0x21, 0x23, 0xDC};
    const uint8_t ST_TimerStart[5] = {0x59, 0x22, 0x00, 0x05, 0x80};
    void processMessages();
    void sendCommand(commands cmd);
    void sendCompass(float heading);
    void sendApparentWindAngle(double angle);
    void sendApparentWindSpeed(double speedKnots);
    void logOptoPins();  // log SeaTalk opto GPIO assignments and current pin states

    // ── Opto GPIO state-change monitoring ──────────────────────────────────
    // Counts edges (state changes) on the opto RX_IN and TX_OUT pins via GPIO
    // interrupts. Intended to show whether there is any activity on the
    // SeaTalk bus. setOptoLog() turns the per-change serial logging on/off.
    void setOptoLog(bool on);
    bool getOptoLog() const;
    // Returns edge counts since the previous call to this method, and resets
    // the counters. Writes rxChanges / txChanges and the elapsed interval (ms).
    void getOptoChanges(unsigned long &rxChanges, unsigned long &txChanges,
                        unsigned long &intervalMs);
    // Drain and emit pending per-change log lines; call from loop().
    void optoLogLoop();

private:
    SoftwareSerial _mySerial;
    SignalManager *_signalManager;

    int checkBus();
    void checkClearToWrite();
    bool send2ST(const uint8_t cmd[], int bytes);
};

#endif
