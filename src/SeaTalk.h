#ifndef SEATALK_H
#define SEATALK_H
#define BEEP_DURATION 150
#define MAX_BUF_SIZE 20
// SH-ESP32 pin mapping for the two-NPN SeaTalk interface (docs/seatalk-sh-esp32-npn.svg).
// The on-board opto block (GPIO35/GPIO33) is no longer used.
//   RX_IN  = GPIO27 -> Q2 collector (pulled up to 3.3 V by R_c). Reads LOW when DATA is 12 V.
//   TX_OUT = GPIO26 -> Q1 base via R_b1. HIGH pulls DATA low.
//   LED_PIN= GPIO2  -> on-board blue LED.
// Both stages invert, so SoftwareSerial is started with invert=true.
#define RX_IN 27
#define TX_OUT 26
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

    // homburg branch: datagrams exactly as in AK-Homberger's
    // ArduinoPilotMicro433WindNG.ino (keystrokes use 86 21, the X=2 format).
    const uint8_t ST_NMEA_BridgeID[3] = {0x90, 0x00, 0xA3};
    const uint8_t ST_Minus_1[4] = {0x86, 0x21, 0x05, 0xFA};
    const uint8_t ST_Minus_10[4] = {0x86, 0x21, 0x06, 0xF9};
    const uint8_t ST_Plus_1[4] = {0x86, 0x21, 0x07, 0xF8};
    const uint8_t ST_Plus_10[4] = {0x86, 0x21, 0x08, 0xF7};
    const uint8_t ST_Auto[4] = {0x86, 0x21, 0x01, 0xFE};
    const uint8_t ST_Standby[4] = {0x86, 0x21, 0x02, 0xFD};
    const uint8_t ST_BeepOn[6] = {0xA8, 0x53, 0x80, 0x00, 0x00, 0xD3};
    const uint8_t ST_BeepOff[6] = {0xA8, 0x43, 0x80, 0x00, 0x00, 0xC3};
    // not in AK's sketch, kept so the existing commands still work
    const uint8_t ST_Wind[4] = {0x86, 0x21, 0x23, 0xDC};
    const uint8_t ST_TimerStart[5] = {0x59, 0x22, 0x00, 0x05, 0x80};
    void processMessages();
    void sendCommand(commands cmd);
    void sendCompass(float heading);
    void sendApparentWindAngle(double angle);
    void sendApparentWindSpeed(double speedKnots);
    void logOptoPins();  // log SeaTalk opto GPIO assignments and current pin states

    // ── Bus activity monitoring ────────────────────────────────────────────
    // Counts bytes received by SoftwareSerial on RX_IN, and edges on TX_OUT (GPIO
    // interrupt). No interrupt is attached to RX_IN: that would replace
    // SoftwareSerial's own receive ISR. setOptoLog() turns the 1 s log on/off.
    void setOptoLog(bool on);
    bool getOptoLog() const;
    // Returns RX bytes / TX edges since the previous call, and resets the window.
    // Writes rxChanges / txChanges and the elapsed interval (ms).
    void getOptoChanges(unsigned long &rxChanges, unsigned long &txChanges,
                        unsigned long &intervalMs);
    // Drain and emit pending per-change log lines; call from loop().
    void optoLogLoop();
    // Diagnostic: sample the raw RX_IN (and TX_OUT) pin levels over a short
    // window and log idle level + min/max seen. Helps tell a dead RX leg from
    // a genuinely silent bus. Blocks ~durationMs; call from main thread only.
    void optoProbe(unsigned long durationMs = 300);
    // Diagnostic: hold TX_OUT HIGH (Q1 on, DATA pulled low) for durationMs so the
    // TX path can be measured with a meter. Non-blocking: released by
    // processMessages(). Logs whether RX_IN saw DATA go low during the hold.
    void txHold(unsigned long durationMs = 10000);
    // Diagnostic: toggle TX_OUT with digitalWrite (no SoftwareSerial) every halfUs
    // and check that RX_IN follows (both stages invert, so RX_IN == TX_OUT).
    // Logs matches and the worst response delay. Blocks for ~40 half-periods.
    void loopbackTest(unsigned long halfUs = 208);
    // Diagnostic: one step each way. TX_OUT HIGH -> time until RX_IN HIGH (DATA pulled
    // low), then TX_OUT LOW -> time until RX_IN LOW (DATA released). Up to 500 ms each.
    void stepTest();

private:
    SoftwareSerial _mySerial;
    SignalManager *_signalManager;
    bool _txHolding = false;
    unsigned long _txHoldStart = 0;    // millis() when the hold began
    unsigned long _txHoldMs = 0;
    int _txHoldRx = -1;                // RX_IN level sampled during the hold

    // AK-Homberger port state (his globals)
    bool _started = false;
    unsigned long _keyTime = 0;        // key_time: last key accepted
    unsigned long _beepTime = 0;       // beep_time: when the beep started
    bool _beepStatus = false;          // beep_status
    unsigned long _beepTimer2 = 0;     // beep_timer2: periodic beep-off
    unsigned long _bridgeTimer = 0;    // bridge_timer: periodic bridge ID

    void txHoldLoop();
    int read9();                                   // Serial1.read() of a 9-bit byte, -1 if none
    int checkWind();                               // AK checkWind()
    bool sendDatagram(const uint8_t data[], int size);   // AK sendDatagram()
    void beepOn();                                 // AK BeepOn()
    void beepOff();                                // AK BeepOff()
    bool send2ST(const uint8_t cmd[], int bytes);  // kept for existing callers
};

#endif
