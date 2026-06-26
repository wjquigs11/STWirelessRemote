#ifndef WINDCLIENT_H
#define WINDCLIENT_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "SeaTalk.h"
#include "Models/WebOptions.h"

// WindClient connects to the Garmin-N2K-Mast-Rotation ESP32 via HTTP,
// fetches the /readings endpoint (JSON with awa, aws fields),
// and translates the readings into SeaTalk protocol messages.

#define WIND_SERVER_PORT 80
#define WIND_POLL_INTERVAL_MS 1000  // Poll every 1 second
#define WIND_RECONNECT_INTERVAL_MS 10000  // Retry connection every 10 seconds on failure

class WindClient
{
public:
    WindClient(SeaTalk *seaTalk);

    // Call from loop() to poll for wind data
    void loop();

    // Print status to serial/webserial
    void printStatus();

    // Set the hostname/IP of the wind server
    void setServerHost(const char *host);

    // Get the current server host
    String getServerHost() { return _serverHost; }

    // Get last received values
    double getLastAWA() { return _lastAWA; }
    double getLastAWS() { return _lastAWS; }
    double getLastSTW() { return _lastSTW; }
    double getLastSOG() { return _lastSOG; }
    double getLastCOG() { return _lastCOG; }
    bool isConnected() { return _connected; }

private:
    SeaTalk *_seaTalk;
    HTTPClient _http;
    String _serverHost;
    String _url;
    bool _httpStarted;
    unsigned long _lastPollTime;
    unsigned long _lastSuccessTime;
    unsigned long _pollInterval;
    unsigned long _reconnectInterval;
    bool _connected;
    double _lastAWA;
    double _lastAWS;
    double _lastSTW;
    double _lastSOG;
    double _lastCOG;
    unsigned long _successCount;
    unsigned long _failCount;

    void fetchReadings();
    void beginHttp();
};

#endif
