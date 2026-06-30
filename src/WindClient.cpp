#include "WindClient.h"
#include "logto.h"

extern char logbuf[];
extern bool wifiConnected;

WindClient::WindClient(SeaTalk *seaTalk)
{
    _seaTalk = seaTalk;
    _serverHost = WIND_SERVER_DEFAULT_HOST;
    _httpStarted = false;
    _lastPollTime = 0;
    _lastSuccessTime = 0;
    _pollInterval = WIND_POLL_INTERVAL_MS;
    _reconnectInterval = WIND_RECONNECT_INTERVAL_MS;
    _connected = false;
    _lastAWA = 0.0;
    _lastAWS = 0.0;
    _lastSTW = 0.0;
    _lastSOG = 0.0;
    _lastCOG = 0.0;
    _successCount = 0;
    _failCount = 0;
}

void WindClient::setServerHost(const char *host)
{
    _serverHost = host;
    // Force reconnect with new host
    if (_httpStarted)
    {
        _http.end();
        _httpStarted = false;
    }
    snprintf(logbuf, LOGBUF_SIZE, "WindClient: server host set to %s", host);
    log::toAll(logbuf);
}

void WindClient::beginHttp()
{
    _url = "http://" + _serverHost + "/readings";
    _http.begin(_url);
    _http.setReuse(true);
    _http.setTimeout(2000);
    _httpStarted = true;
}

void WindClient::loop()
{
    if (!wifiConnected) return;

    unsigned long now = millis();
    unsigned long interval = _connected ? _pollInterval : _reconnectInterval;

    if (now - _lastPollTime >= interval)
    {
        _lastPollTime = now;
        fetchReadings();
    }
}

void WindClient::fetchReadings()
{
    if (!_httpStarted)
        beginHttp();

    int httpCode = _http.GET();

    if (httpCode == HTTP_CODE_OK)
    {
        String payload = _http.getString();
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (!error)
        {
            // The Garmin-N2K-Mast-Rotation readings[] contains:
            // "awa" - Apparent Wind Angle in degrees
            // "aws" - Apparent Wind Speed in knots
            bool gotWind = false;

            if (doc["awa"].is<const char*>() || doc["awa"].is<float>() || doc["awa"].is<double>())
            {
                double awa;
                if (doc["awa"].is<const char*>())
                    awa = atof(doc["awa"].as<const char*>());
                else
                    awa = doc["awa"].as<double>();

                _lastAWA = awa;
                _seaTalk->sendApparentWindAngle(awa);
                gotWind = true;
            }

            if (doc["aws"].is<const char*>() || doc["aws"].is<float>() || doc["aws"].is<double>())
            {
                double aws;
                if (doc["aws"].is<const char*>())
                    aws = atof(doc["aws"].as<const char*>());
                else
                    aws = doc["aws"].as<double>();

                _lastAWS = aws;
                _seaTalk->sendApparentWindSpeed(aws);
                gotWind = true;
            }

            if (doc["stw"].is<const char*>() || doc["stw"].is<float>() || doc["stw"].is<double>())
            {
                double stw;
                if (doc["stw"].is<const char*>())
                    stw = atof(doc["stw"].as<const char*>());
                else
                    stw = doc["stw"].as<double>();
                _lastSTW = stw;
                gotWind = true;
            }

            if (doc["sog"].is<const char*>() || doc["sog"].is<float>() || doc["sog"].is<double>())
            {
                double sog;
                if (doc["sog"].is<const char*>())
                    sog = atof(doc["sog"].as<const char*>());
                else
                    sog = doc["sog"].as<double>();
                _lastSOG = sog;
                gotWind = true;
            }

            if (doc["cog"].is<const char*>() || doc["cog"].is<float>() || doc["cog"].is<double>())
            {
                double cog;
                if (doc["cog"].is<const char*>())
                    cog = atof(doc["cog"].as<const char*>());
                else
                    cog = doc["cog"].as<double>();
                _lastCOG = cog;
                gotWind = true;
            }

            if (gotWind)
            {
                _connected = true;
                _lastSuccessTime = millis();
                _successCount++;
            }
        }
        else
        {
            Serial.printf("WindClient: JSON parse error: %s\n", error.c_str());
            _failCount++;
            _connected = false;
        }
    }
    else
    {
        if (httpCode > 0)
            Serial.printf("WindClient: HTTP error %d\n", httpCode);
        else
            Serial.printf("WindClient: connection failed: %s\n", _http.errorToString(httpCode).c_str());
        _failCount++;
        _connected = false;
        // Reset so next attempt re-establishes the connection
        _http.end();
        _httpStarted = false;
    }
}

void WindClient::printStatus()
{
    snprintf(logbuf, LOGBUF_SIZE, "WindClient: server=%s connected=%s",
        _serverHost.c_str(), _connected ? "yes" : "no");
    log::toAll(logbuf);
    snprintf(logbuf, LOGBUF_SIZE, "WindClient: AWA=%.1f AWS=%.1f knots",
        _lastAWA, _lastAWS);
    log::toAll(logbuf);
    snprintf(logbuf, LOGBUF_SIZE, "WindClient: success=%lu fail=%lu lastSuccess=%lus ago",
        _successCount, _failCount,
        _lastSuccessTime > 0 ? (millis() - _lastSuccessTime) / 1000 : 0);
    log::toAll(logbuf);
}
