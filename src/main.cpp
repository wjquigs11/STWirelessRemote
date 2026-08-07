#include "include.h"

Preferences preferences;
#ifdef DRD
bool doubleReset = false;
#endif
bool OTAinProgress = false;

unsigned long ota_progress_millis = 0;

unsigned long now;

void onOTAStart() {
  Serial.println("OTA update started!");
  OTAinProgress = true;
}

void onOTAProgress(size_t current, size_t final) {
  if (now - ota_progress_millis > 1000) {
    ota_progress_millis = now;
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

void onOTAEnd(bool success) {
  OTAinProgress = false;
  if (success) Serial.println("OTA update finished successfully!");
  else Serial.println("There was an error during OTA update!");
}

#ifdef WIFI
bool wifiEnabled = true;
bool wifiConnected = false;
unsigned long wifiStartTime = 0;
String host;
#else
bool wifiEnabled = false;
bool wifiConnected = false;
#endif

int timerDelay = 1000;
int loopDelay = 10;
time_t lastUpdate, updateTime;
unsigned long lastTime = 0;
unsigned long loopCount, maxLoopCount, webLoopCount;
struct tm *ptm;
char prbuf[PRBUF];

#ifdef SEATALK
SeaTalk *seatalk = nullptr;
RemoteControl *remoteControl = nullptr;
Options *options = nullptr;
SeaTalkData *seaTalkData = nullptr;
SignalManager *signalManager = nullptr;
bool seatalkDebug = false;
WindClient *windClient = nullptr;
#endif

#ifdef DEEPSLEEP
int awakeTimer = 300;
RTC_DATA_ATTR int bootCount = 0;

void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();
  switch(wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0: log::toAll("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1: log::toAll("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER: log::toAll("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD: log::toAll("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP: log::toAll("Wakeup caused by ULP program"); break;
    default:
      snprintf(logbuf, LOGBUF_SIZE, "Wakeup was not caused by deep sleep: %d", (int)wakeup_reason);
      log::toAll(logbuf);
      break;
  }
}
#else
int bootCount = 0;
#endif
unsigned long startTime;

// Forward declaration for web command processing
extern void processWebCommands();

void setup() {
  Serial.begin(115200);
  delay(5000);
  startTime = millis();

  setup_custom_panic_handler();

  // Mount filesystem
  if (LittleFS.begin(false, "/littlefs", 10, "littlefs")) {
    Serial.println("opened LittleFS");
  } else {
    Serial.println("failed to open LittleFS - trying format");
    if (LittleFS.begin(true, "/littlefs", 10, "littlefs")) {
      Serial.println("LittleFS formatted and mounted");
    } else {
      Serial.println("LittleFS mount failed even after format");
    }
  }

#ifdef WIFI
  checkLittleFS();
  gotWifiCreds = readWiFiCredentials();
#endif
  if (!log::initConsole()) {
    log::toAll("failed to open console log");
  } else {
    log::toAll("console log open");
  }

  ++bootCount;
  snprintf(logbuf, LOGBUF_SIZE, "Boot number: %d", bootCount);
  log::toAll(logbuf);

#ifdef DEEPSLEEP
  print_wakeup_reason();
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  snprintf(logbuf, LOGBUF_SIZE, "Setup ESP32 to sleep for %d Seconds every %d seconds", TIME_TO_SLEEP, awakeTimer);
  log::toAll(logbuf);
#endif

  preferences.begin("ESPprefs", false);
  timerDelay = preferences.getInt("timerdelay", 10000);
  if (timerDelay < 100) {
    timerDelay = 100;
    preferences.putInt("timerdelay", 100);
  }
  snprintf(logbuf, LOGBUF_SIZE, "timerDelay %d", timerDelay);
  log::toAll(logbuf);
  wifiEnabled = preferences.getBool("wifi", true);

#ifdef SEATALK
  // Initialize SeaTalk components
  log::toAll("Initializing SeaTalk...");
  seaTalkData = new SeaTalkData();
  options = new Options();
  signalManager = new SignalManager(seaTalkData);
  seatalk = new SeaTalk(signalManager);
  remoteControl = new RemoteControl(seatalk, options);
  WebOptions webOpts = options->GetWebOptions();
  seatalkDebugRx = webOpts.seatalkDebugRx;
  seatalkDebugTx = webOpts.seatalkDebugTx;
  log::toAll("SeaTalk initialized");

  // Initialize WindClient (connects to Garmin-N2K-Mast-Rotation ESP32)
  windClient = new WindClient(seatalk);
  windClient->setServerHost(webOpts.windhost.c_str());
  log::toAll("WindClient initialized");
#ifdef N2K
  headingOffset = webOpts.headingOffset;
  xmitHeading = webOpts.xmitHeading;
  windXmitHz = webOpts.windXmitHz;
#endif
#endif

#ifdef N2K
  n2kSetup();
#endif

#ifdef WIFI
  host = preferences.getString("hostname", "ESPmcu");
  snprintf(logbuf, LOGBUF_SIZE, "hostname: %s", host.c_str());
  log::toAll(logbuf);
#ifdef DRD
  doubleReset = preferences.getBool("DRD", false);
#endif
  if (wifiEnabled) {
#ifdef DRD
    if (doubleReset) {
      log::toAll("double reset detected - starting captive portal");
      preferences.putBool("DRD", false);
      doubleReset = false;
      preferences.end();
      startPortal();
      int remaining = DRD_TIMEOUT;
      while (remaining > 0) {
        delay(1000);
        dnsServer.processNextRequest();
        if (WiFi.softAPgetStationNum() > 0) {
          Serial.println("DRD: client connected - portal active, waiting for config");
          while (true) {
            delay(10);
            dnsServer.processNextRequest();
          }
        }
        Serial.printf("DRD: no clients - rebooting in %d seconds (connect to ESP-SETUP to configure WiFi)\n", remaining);
        remaining--;
      }
      log::toAll("DRD: timeout with no clients - rebooting to normal WiFi mode");
      delay(500);
      ESP.restart();
    } else {
      preferences.putBool("DRD", true);
#endif // DRD
      setupWifi();
#ifdef WEB
      startWebServer();
      serverStarted = true;
#endif
#ifdef WEBSERIAL
      WebSerial.begin(&server);
      WebSerial.onMessage(WebSerialonMessage);
#endif
#ifdef ELEGANTOTA
      ElegantOTA.begin(&server);
      ElegantOTA.onStart(onOTAStart);
      ElegantOTA.onProgress(onOTAProgress);
      ElegantOTA.onEnd(onOTAEnd);
#endif
#ifdef DRD
    }
#endif // DRD
  }
#endif // WIFI

  consLog.flush();

  // Timer-based event sending handled in loop() via millis()
}

void loop() {
  now = millis();
  static unsigned long lastEventTime, lastTimeTime, startTime;
  loopCount++;
#ifdef DRD
  if (doubleReset) {
    static bool drdCleared = false;
    static unsigned long lastDrdLog = 0;
    if (!drdCleared) {
      int remaining = (DRD_TIMEOUT * 1000 - now) / 1000 + 1;
      if (remaining > 0 && (now - lastDrdLog >= 1000)) {
        snprintf(logbuf, LOGBUF_SIZE, "DRD: %ds remaining - reset again to trigger WIFI AP mode", remaining);
        log::toAll(logbuf);
        lastDrdLog = now;
      }
    }
    if (!drdCleared && (now > (DRD_TIMEOUT * 1000))) {
      preferences.putBool("DRD", false);
      drdCleared = true;
      doubleReset = false;
      log::toAll("DRD timeout - cleared double reset flag");
#if defined(WIFI) && defined(WEB)
      startWebServer();
      serverStarted = true;
#endif
#ifdef WEBSERIAL
      WebSerial.begin(&server);
      WebSerial.onMessage(WebSerialonMessage);
#endif
    }
  } else {
#endif // DRD
    if (!OTAinProgress) {
      // Periodic sensor update (replaces ReactESP onRepeat)
      if (now - lastTime >= (unsigned long)timerDelay) {
        lastTime = now;
        loopCount = 0;
#if defined(WIFI) && defined(WEB)
        events.send(getSensorReadings().c_str(), "new_readings", now);
#endif
      }
      if (loopCount > maxLoopCount) maxLoopCount = loopCount;
      webLoopCount++;
#ifdef WEBSERIAL
      WebSerial.loop();
      pollSerialConsole();
#endif
#ifdef WIFI
      if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA)
        dnsServer.processNextRequest();
#endif

#ifdef SEATALK
      // Process SeaTalk bus messages
      seatalk->processMessages();
      // Process RF remote control
      remoteControl->processRemote();
      // Process web commands on main thread
      processWebCommands();
      // Process wind client (SSE from Garmin-N2K-Mast-Rotation)
      if (windClient) windClient->loop();
#endif

#ifdef N2K
      n2kLoop();
#endif

#ifdef DEEPSLEEP
      if ((now - startTime) > (awakeTimer * 1000)) {
        log::toAll("Going to sleep in 5 seconds...");
        consLog.flush();
#ifdef WEBSERIAL
        WebSerial.flush();
#endif
        delay(5000);
        snprintf(logbuf, LOGBUF_SIZE, "Entering deep sleep for %d seconds", TIME_TO_SLEEP);
        log::toAll(logbuf);
        esp_deep_sleep_start();
      }
#endif
    } // !OTAinProgress
#ifdef DRD
  } // else no doubleReset
#endif
#ifdef ELEGANTOTA
  ElegantOTA.loop();
#endif
}

// WindClient status function for webserial
#ifdef SEATALK
void windClientStatus() {
  if (windClient) {
    windClient->printStatus();
  } else {
    log::toAll("WindClient not initialized");
  }
}
#endif
