#include <Arduino.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <time.h>
#include <Wire.h>

#ifdef DEEPSLEEP
// ESP32 deep sleep includes
#include "esp_system.h"
#include "esp_sleep.h"
#endif

#ifdef WIFI
#include <WiFi.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
extern AsyncWebServer server;
extern AsyncEventSource events;
extern bool serverStarted;
extern bool wifiConnected;
extern String host;
extern unsigned long wifiStartTime;
extern TimerHandle_t connectTimer;
extern int timerDelay;
#define HTTP_PORT 80
void checkLittleFS();
bool readWiFiCredentials();
extern bool gotWifiCreds;
// for captive portal
#include <DNSServer.h>
extern DNSServer dnsServer;
#define DNS_INTERVAL 300
bool setupWifi();
void resetWifi();
void saveNewWifiCredentials(const char* ssid, const char* password);
void deleteWifiCredentials(const char* ssid);
extern String newWifiSsid;
extern String newWifiPass;
// WiFi credentials
struct WiFiCredentials {
  String ssid;
  String password;
  String ip;
  String gateway;
};
#define MAX_WIFI_NETWORKS 5
extern WiFiCredentials wifi[];
extern int wifiCount;
#ifdef WEB
void startWebServer();
String getSensorReadings();
#endif
String processor(const String& var);
void startPortal();
void onWiFiConnected();
void onWiFiDisconnected();
#endif
#ifdef DRD
// longer timeout gives user more time to decide whether to start an OTA upload
// but also means longer delay after second reboot, if user did not intend
#define DRD_TIMEOUT 30
#endif

#ifdef WEBSERIAL
#include <WebSerialPro.h>
void WebSerialonMessage(uint8_t *data, size_t len);
extern String appCommandList[];
extern String appToggleList[];
using Handler = void(*)(String*, int);
extern Handler appHandler;
extern Handler togHandler;
void pollSerialConsole();
#endif
#ifdef ELEGANTOTA
#define ELEGANTOTA_USE_ASYNC_WEBSERVER 1
#include <ElegantOTA.h>
#endif

#include "logto.h"

extern Preferences preferences;
extern File consLog;

// Timer variables
#include <time.h>
#define DEFDELAY 1000
extern unsigned long lastTime;
// timerDelay defines how long we wait to send an update to connected web clients
extern int timerDelay;
// loopDelay defines how long we delay at the end of each iteration of loop();
// if we're not doing captive portal (for dns) it can be 0, although spinning on CPU for most of my projects seems a bit silly
extern int loopDelay;
extern int minReadRate;
// store last update based on clock time from client browser
extern time_t lastUpdate, updateTime;
extern struct tm *ptm;
#define PRBUF 128
extern char prbuf[];
extern unsigned long loopCount, maxLoopCount, webLoopCount;

#ifdef NTP
// Time synchronization functions
void setupTime();
void waitForTimeSync(int timeoutSeconds = 10);
String getFormattedTime();
unsigned long getEpochTime();
bool isNtpSyncSuccessful();
void resyncNTP();
#endif

// Custom panic handler setup
void setup_custom_panic_handler();

#ifdef DEEPSLEEP
// Deep sleep variables
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  60       /* Time ESP32 will go to sleep (in seconds) */
extern int awakeTimer;          /* Time to stay awake before going to sleep (in seconds) */
extern unsigned long startTime; /* Time when the device started */

// Function to print the wakeup reason
void print_wakeup_reason();
#endif

extern unsigned long now;

// N2K CAN bus (optional alternate wind source)
#ifdef N2K
extern bool n2kDebug;
extern bool n2kOpen;
extern bool n2kWindActive;
extern unsigned long n2kMsgCount;
extern unsigned long n2kWindCount;
extern unsigned long n2kLastWindTime;
extern double n2kLastAWA;
extern double n2kLastAWS;
void n2kSetup();
void n2kLoop();
void n2kStatus();
#endif

// SeaTalk
#ifdef SEATALK
#include "SeaTalk.h"
#include "RemoteControl.h"
#include "Options.h"
#include "WindClient.h"
extern SeaTalk *seatalk;
extern RemoteControl *remoteControl;
extern Options *options;
extern SeaTalkData *seaTalkData;
extern SignalManager *signalManager;
extern WindClient *windClient;
#endif
