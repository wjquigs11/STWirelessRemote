#ifdef N2K
/*
  N2K CAN bus for STW-wjquigs.
  Receives Wind PGN 130306 and optionally transmits Heading PGN 127250.
  Provides an alternate wind data source (in addition to TCP WindClient).
  When wind data arrives via N2K, it is fed to the SignalManager which
  updates SeaTalkData and sends to the SeaTalk bus.
*/
#include "include.h"
#include <N2kMessages.h>
#include <NMEA2000.h>
#include <NMEA2000_esp32.h>

// ─── Configuration ─────────────────────────────────────────────────────────────
// ESP32 native CAN bus pins - adjust for your hardware
#define N2K_CAN_TX_PIN GPIO_NUM_26
#define N2K_CAN_RX_PIN GPIO_NUM_27

#define DEGTORAD 0.0174532925

// ─── State ─────────────────────────────────────────────────────────────────────
tNMEA2000 *n2kBus = nullptr;
bool n2kDebug = false;
bool n2kOpen = false;
bool n2kWindActive = false;  // true once we receive at least one wind PGN

// Heading transmit settings
int headingOffset = 0;       // degrees to add to compass heading
bool xmitHeading = false;    // transmit heading on N2K bus

// Wind transmit throttle
float windXmitHz = 1.0f;    // wind transmission rate on SeaTalk (Hz)

// Statistics
unsigned long n2kMsgCount = 0;      // total messages received
unsigned long n2kWindCount = 0;     // wind messages received
unsigned long n2kLastWindTime = 0;

unsigned long n2kMsgSentCount = 0;  // total messages sent
unsigned long n2kWindSentCount = 0; // wind messages sent

// Last received values
double n2kLastAWA = 0.0;  // degrees
double n2kLastAWS = 0.0;  // knots

#define MTOKTS 1.9438444924
#define RADTODEG 57.2957795131

// ─── Callbacks ─────────────────────────────────────────────────────────────────

static void OnN2kOpen() {
  n2kOpen = true;
  log::toAll("N2K: CAN bus open");
}

static void HandleN2kMsg(const tN2kMsg &N2kMsg) {
  n2kMsgCount++;

  if (n2kDebug) {
    snprintf(logbuf, LOGBUF_SIZE, "N2K PGN %lu src %d len %d", N2kMsg.PGN, N2kMsg.Source, N2kMsg.DataLen);
    log::toAll(logbuf);
  }

  switch (N2kMsg.PGN) {
    case 130306L: { // Wind Data
      unsigned char SID;
      double windSpeedMs;
      double windAngleRad;
      tN2kWindReference wRef;

      if (ParseN2kPGN130306(N2kMsg, SID, windSpeedMs, windAngleRad, wRef)) {
        // Only process apparent wind
        if (wRef != N2kWind_Apparent) {
          if (n2kDebug) {
            snprintf(logbuf, LOGBUF_SIZE, "N2K: ignoring non-apparent wind ref=%d", (int)wRef);
            log::toAll(logbuf);
          }
          return;
        }

        n2kWindCount++;
        n2kLastWindTime = millis();
        n2kWindActive = true;

        double awaDeg = windAngleRad * RADTODEG;
        double awsKts = windSpeedMs * MTOKTS;

        n2kLastAWA = awaDeg;
        n2kLastAWS = awsKts;

        if (n2kDebug) {
          snprintf(logbuf, LOGBUF_SIZE, "N2K Wind: AWA=%.1f deg AWS=%.1f kts", awaDeg, awsKts);
          log::toAll(logbuf);
        }

        // Feed to SignalManager -> SeaTalkData -> SeaTalk bus
#ifdef SEATALK
        if (signalManager) {
          signalManager->UpdateApparentWindAngle(awaDeg);
          signalManager->UpdateApparentWindSpeed(awsKts);
        }
        // Throttle wind transmission on SeaTalk bus
        {
          static unsigned long lastN2kWindXmitTime = 0;
          unsigned long nowMs = millis();
          unsigned long intervalMs = (windXmitHz > 0) ? (unsigned long)(1000.0f / windXmitHz) : 1000;
          if (seatalk && (nowMs - lastN2kWindXmitTime >= intervalMs)) {
            lastN2kWindXmitTime = nowMs;
            seatalk->sendApparentWindAngle(awaDeg);
            delay(50);
            seatalk->sendApparentWindSpeed(awsKts);
          }
        }
#else
        // No SeaTalk — values are stored in n2kLastAWA/n2kLastAWS for other consumers
        (void)awaDeg;
        (void)awsKts;
#endif
      }
      break;
    }
    default:
      break;
  }
}

// ─── Public API ────────────────────────────────────────────────────────────────

void n2kSetup() {
  log::toAll("N2K: initializing CAN bus...");

  n2kBus = new tNMEA2000_esp32(N2K_CAN_TX_PIN, N2K_CAN_RX_PIN);

  n2kBus->SetN2kCANReceiveFrameBufSize(150);
  n2kBus->SetN2kCANSendFrameBufSize(50);

  // Product information
  n2kBus->SetProductInformation(
    "20250718",
    100,
    "STW N2K Bridge",
    "1.0.0",
    "1.0.0"
  );

  // Device information
  n2kBus->SetDeviceInformation(
    20250718,  // serial
    130,       // function: atmospheric
    85,        // class: external environment
    2046       // manufacturer
  );

  // ListenAndNode: receive all messages and can transmit
  n2kBus->SetMode(tNMEA2000::N2km_ListenAndNode);
  n2kBus->EnableForward(false);
  n2kBus->SetMsgHandler(HandleN2kMsg);
  n2kBus->SetOnOpen(OnN2kOpen);

  n2kBus->Open();

  if (n2kOpen) {
    log::toAll("N2K: bus opened successfully");
  } else {
    log::toAll("N2K: bus open initiated (async)");
  }
}

void n2kLoop() {
  if (n2kBus) {
    n2kBus->ParseMessages();
  }
}

void n2kStatus() {
  snprintf(logbuf, LOGBUF_SIZE, "N2K: open=%s active=%s debug=%s xmitHdg=%s offset=%d",
    n2kOpen ? "yes" : "no",
    n2kWindActive ? "yes" : "no",
    n2kDebug ? "yes" : "no",
    xmitHeading ? "yes" : "no",
    headingOffset);
  log::toAll(logbuf);
  snprintf(logbuf, LOGBUF_SIZE, "N2K: rx_msgs=%lu rx_wind=%lu tx_msgs=%lu tx_wind=%lu",
    n2kMsgCount, n2kWindCount, n2kMsgSentCount, n2kWindSentCount);
  log::toAll(logbuf);
  snprintf(logbuf, LOGBUF_SIZE, "N2K: lastWind=%lus ago",
    n2kLastWindTime > 0 ? (millis() - n2kLastWindTime) / 1000 : 0);
  log::toAll(logbuf);
  if (n2kWindActive) {
    snprintf(logbuf, LOGBUF_SIZE, "N2K: lastAWA=%.1f deg lastAWS=%.1f kts",
      n2kLastAWA, n2kLastAWS);
    log::toAll(logbuf);
  }
}

void n2kSendHeading(double headingDeg) {
  if (!n2kBus || !n2kOpen || !xmitHeading) return;

  // Apply offset and normalize to 0-360
  double adjusted = headingDeg + headingOffset;
  while (adjusted < 0) adjusted += 360.0;
  while (adjusted >= 360.0) adjusted -= 360.0;

  tN2kMsg n2kMsg;
  SetN2kPGN127250(n2kMsg, 255, adjusted * DEGTORAD, N2kDoubleNA, N2kDoubleNA, N2khr_magnetic);

  if (n2kBus->SendMsg(n2kMsg)) {
    n2kMsgSentCount++;
    if (n2kDebug) {
      snprintf(logbuf, LOGBUF_SIZE, "N2K TX: PGN127250 hdg=%.1f (raw=%.1f offset=%d)",
        adjusted, headingDeg, headingOffset);
      log::toAll(logbuf);
    }
  } else {
    if (n2kDebug) {
      log::toAll("N2K TX FAIL: PGN127250");
    }
  }
}

#endif // N2K
