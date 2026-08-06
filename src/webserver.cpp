#if defined(WIFI)
#include "include.h"

AsyncWebServer server(HTTP_PORT);
AsyncEventSource events("/events");

bool serverStarted;
#endif

#if defined(WIFI) && defined(WEB)

// ─── App-specific web server ───────────────────────────────────────────────────

#ifdef SEATALK
#include "CommandStack.h"
static CommandStack commandStack;
#endif

void startAppWebServer() {
#ifdef SEATALK
  // SeaTalk command endpoint (from STW-wjquigs)
  server.on("/SendCommand.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("action")) {
      const AsyncWebParameter *p = request->getParam("action");
      Serial.println("Action Received");
      Serial.println(p->value());
      if (p->value() == "minus1") {
        commandStack.push(minus_1);
      } else if (p->value() == "plus1") {
        commandStack.push(plus_1);
      } else if (p->value() == "minus10") {
        commandStack.push(minus_10);
      } else if (p->value() == "plus10") {
        commandStack.push(plus_10);
      } else if (p->value() == "auto") {
        commandStack.push(auto_but);
      } else if (p->value() == "standby") {
        commandStack.push(standby_but);
      } else if (p->value() == "starttimer") {
        commandStack.push(start_timer);
      } else if (p->value() == "wind") {
        commandStack.push(wind_mode);
      } else if (p->value() == "tackport") {
        commandStack.push(tack_port);
      } else if (p->value() == "tackstarboard") {
        commandStack.push(tack_starboard);
      }
    }
    request->send(200);
  });

  // Get/Set button options
  server.on("/GetOptions.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    WebOptions webOptions = options->GetWebOptions();
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    response->print("{  \n");
    response->printf("\"button1opt\": %d, \n", webOptions.button1);
    response->printf("\"button2opt\": %d, \n", webOptions.button2);
    response->printf("\"button3opt\": %d, \n", webOptions.button3);
    response->printf("\"button4opt\": %d, \n", webOptions.button4);
    response->printf("\"button5opt\": %d, \n", webOptions.button5);
    response->printf("\"button6opt\": %d, \n", webOptions.button6);
    response->printf("\"button7opt\": %d, \n", webOptions.button7);
    response->printf("\"button8opt\": %d, \n", webOptions.button8);
    response->printf("\"timermin\": %d, \n", webOptions.timermin);
    response->printf("\"timersec\": %d, \n", webOptions.timersec);
    response->printf("\"windhost\": \"%s\", \n", webOptions.windhost.c_str());
    response->printf("\"windtcp\": %s, \n", (windClient && windClient->enabled) ? "true" : "false");
    response->printf("\"seatalkDebugRx\": %s, \n", webOptions.seatalkDebugRx ? "true" : "false");
    response->printf("\"seatalkDebugTx\": %s \n", webOptions.seatalkDebugTx ? "true" : "false");
    response->print("}");
    request->send(response);
  });

  server.on("/SetOptions.html", HTTP_POST, [](AsyncWebServerRequest *request) {
    WebOptions webOptions = options->GetWebOptions();
    if (request->hasParam("button1act", true)) webOptions.button1 = request->getParam("button1act", true)->value().toInt();
    if (request->hasParam("button2act", true)) webOptions.button2 = request->getParam("button2act", true)->value().toInt();
    if (request->hasParam("button3act", true)) webOptions.button3 = request->getParam("button3act", true)->value().toInt();
    if (request->hasParam("button4act", true)) webOptions.button4 = request->getParam("button4act", true)->value().toInt();
    if (request->hasParam("button5act", true)) webOptions.button5 = request->getParam("button5act", true)->value().toInt();
    if (request->hasParam("button6act", true)) webOptions.button6 = request->getParam("button6act", true)->value().toInt();
    if (request->hasParam("button7act", true)) webOptions.button7 = request->getParam("button7act", true)->value().toInt();
    if (request->hasParam("button8act", true)) webOptions.button8 = request->getParam("button8act", true)->value().toInt();
    if (request->hasParam("timermin", true)) webOptions.timermin = request->getParam("timermin", true)->value().toInt();
    if (request->hasParam("timersec", true)) webOptions.timersec = request->getParam("timersec", true)->value().toInt();
    if (request->hasParam("windhost", true)) webOptions.windhost = request->getParam("windhost", true)->value();
    if (request->hasParam("seatalkDebugRx", true)) webOptions.seatalkDebugRx = request->getParam("seatalkDebugRx", true)->value() == "on";
    if (request->hasParam("seatalkDebugTx", true)) webOptions.seatalkDebugTx = request->getParam("seatalkDebugTx", true)->value() == "on";
    options->SaveWebOptions(webOptions);
    if (windClient) windClient->setServerHost(webOptions.windhost.c_str());
    request->send(200);
  });

  // Wind TCP toggle endpoint
  server.on("/windtcp", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("enabled")) {
      bool en = request->getParam("enabled")->value() == "1";
      if (windClient) {
        windClient->setEnabled(en);
        snprintf(logbuf, LOGBUF_SIZE, "WindClient TCP: %s (via web)", en ? "enabled" : "disabled");
        log::toAll(logbuf);
      }
      request->send(200, "text/plain", en ? "enabled" : "disabled");
    } else {
      // No param: just return current state
      request->send(200, "text/plain", (windClient && windClient->enabled) ? "enabled" : "disabled");
    }
  });
#endif
}

static JsonDocument readings;
static char readingsBuf[128];

String getSensorReadings() {
  readings.clear();
#ifdef SEATALK
  // Use SeaTalk bus data if available, otherwise fall back to WindClient
  readings["awa"] = (seaTalkData->apparentWindAngle != 0.0) ? seaTalkData->apparentWindAngle
                    : (windClient && windClient->isConnected()) ? windClient->getLastAWA() : 0.0;
  readings["aws"] = (seaTalkData->apparentWindSpeed != 0.0) ? seaTalkData->apparentWindSpeed
                    : (windClient && windClient->isConnected()) ? windClient->getLastAWS() : 0.0;
  readings["stw"] = (seaTalkData->speedThroughWater != 0.0) ? seaTalkData->speedThroughWater
                    : (windClient && windClient->isConnected()) ? windClient->getLastSTW() : 0.0;
  readings["sog"] = (seaTalkData->speedOverGround != 0.0) ? seaTalkData->speedOverGround
                    : (windClient && windClient->isConnected()) ? windClient->getLastSOG() : 0.0;
  readings["cog"] = (seaTalkData->courseOverGround != 0.0) ? seaTalkData->courseOverGround
                    : (windClient && windClient->isConnected()) ? windClient->getLastCOG() : 0.0;
  if (seaTalkData->compassHeading != 0.0)
    readings["hdg"] = seaTalkData->compassHeading;
#else
  readings["sensor"] = "0";
#endif
  serializeJson(readings, readingsBuf, sizeof(readingsBuf));
  return String(readingsBuf);
}

// Process commands from the web interface on the main thread
void processWebCommands() {
#ifdef SEATALK
  while (!commandStack.isEmpty()) {
    commands nextCommand = (commands)commandStack.pop();
    seatalk->sendCommand(nextCommand);
  }
#endif
}

// ─── N2K Status JSON endpoint ──────────────────────────────────────────────────

String getN2kStatusJson() {
  JsonDocument doc;
#ifdef N2K
  doc["n2k_rx_msgs"] = n2kMsgCount;
  doc["n2k_rx_wind"] = n2kWindCount;
  doc["n2k_tx_msgs"] = n2kMsgSentCount;
  doc["n2k_tx_wind"] = n2kWindSentCount;
  doc["n2k_open"] = n2kOpen;
  doc["n2k_active"] = n2kWindActive;
  doc["n2k_last_wind_s"] = n2kLastWindTime > 0 ? (millis() - n2kLastWindTime) / 1000 : 0;
  doc["n2k_last_awa"] = n2kLastAWA;
  doc["n2k_last_aws"] = n2kLastAWS;
  doc["seatalkDebugRx"] = seatalkDebugRx;
  doc["seatalkDebugTx"] = seatalkDebugTx;
#else
  doc["n2k_enabled"] = false;
  doc["seatalkDebugRx"] = false;
  doc["seatalkDebugTx"] = false;
#endif
  String json;
  serializeJson(doc, json);
  return json;
}

// ─── Main web server setup ─────────────────────────────────────────────────────

void startWebServer() {
  log::toAll("starting web server");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html", false, processor);
  });

  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  server.on("/host", HTTP_GET, [](AsyncWebServerRequest *request) {
    snprintf(logbuf, LOGBUF_SIZE, "hostname: %s, MAC: %s", host.c_str(), WiFi.macAddress().c_str());
    log::toAll(logbuf);
    request->send(200, "text/plain", logbuf);
  });

  // N2K status endpoint
  server.on("/n2k.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = getN2kStatusJson();
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
  });

  // Client time endpoint
  server.on("/clienttime", HTTP_POST, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "Time received");
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
  },
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
#ifdef NTP
      if (!isNtpSyncSuccessful()) {
        log::toAll("Using client time as NTP sync failed");
        JsonDocument doc;
        ::deserializeJson(doc, data);
        String clientTime = doc["datetime"];
        int year, month, day, hour, minute, second;
        sscanf(clientTime.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d", &year, &month, &day, &hour, &minute, &second);
        struct tm timeinfo;
        timeinfo.tm_year = year - 1900;
        timeinfo.tm_mon  = month - 1;
        timeinfo.tm_mday = day;
        timeinfo.tm_hour = hour;
        timeinfo.tm_min  = minute;
        timeinfo.tm_sec  = second;
        time_t t = mktime(&timeinfo);
        struct timeval now = { .tv_sec = t, .tv_usec = 0 };
        settimeofday(&now, NULL);
      }
#else
      log::toAll("Received client time.");
#endif
    }
  );

  // WiFi management routes (also registered in captive portal)
  server.on("/wifimanager", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/wifimanager.html", "text/html");
  });

  server.on("/wifimanager", HTTP_POST, [](AsyncWebServerRequest *request) {
    String ssid, password, ip, gateway;
    int params = request->params();
    for (int i = 0; i < params; i++) {
      const AsyncWebParameter* p = request->getParam(i);
      if (p->isPost()) {
        if (p->name() == "ssid") ssid = p->value();
        else if (p->name() == "pass") password = p->value();
        else if (p->name() == "ip") ip = p->value();
        else if (p->name() == "gateway") gateway = p->value();
        else if (p->name() == "hostname" && p->value().length() > 0) {
          host = p->value();
          preferences.putString("hostname", host);
        }
      }
    }
    if (ssid.length() > 0 && password.length() > 0) {
      saveNewWifiCredentials(ssid.c_str(), password.c_str());
      request->send(200, "text/plain", "WiFi credentials saved. Restart to connect to new network.");
    } else {
      request->send(400, "text/plain", "Must supply ssid and password");
    }
  });

  server.on("/wifilist", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "[";
    for (int i = 0; i < wifiCount; i++) {
      if (i > 0) json += ",";
      json += "\"";
      json += wifi[i].ssid;
      json += "\"";
    }
    json += "]";
    request->send(200, "application/json", json);
  });

  server.on("/wifidelete", HTTP_POST, [](AsyncWebServerRequest *request) {
    int params = request->params();
    int deleted = 0;
    for (int i = 0; i < params; i++) {
      const AsyncWebParameter* p = request->getParam(i);
      if (p->isPost() && p->name() == "ssid") {
        deleteWifiCredentials(p->value().c_str());
        deleted++;
      }
    }
    snprintf(logbuf, LOGBUF_SIZE, "Deleted %d SSID(s) via web", deleted);
    log::toAll(logbuf);
    request->send(200, "text/plain", "OK");
  });

  events.onConnect([](AsyncEventSourceClient *client) {
    if (client->lastId()) {
      Serial.printf("Client reconnected! Last message ID: %u\n", client->lastId());
    }
    client->send("hello!", NULL, now, 1000);
  });

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type, Cache-Control");

  server.addHandler(&events);

  // CORS preflight handlers
  server.on("/events", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(204);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type, Cache-Control");
    response->addHeader("Access-Control-Max-Age", "3600");
    request->send(response);
  });

  server.on("/clienttime", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(204);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    response->addHeader("Access-Control-Max-Age", "3600");
    request->send(response);
  });

  server.onNotFound([](AsyncWebServerRequest *request) {
    if (request->method() == HTTP_OPTIONS) {
      AsyncWebServerResponse *response = request->beginResponse(204);
      response->addHeader("Access-Control-Allow-Origin", "*");
      response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
      response->addHeader("Access-Control-Allow-Headers", "Content-Type");
      response->addHeader("Access-Control-Max-Age", "3600");
      request->send(response);
      return;
    }
    // Try appending .html for extensionless URLs
    String path = request->url();
    if (!path.endsWith("/") && LittleFS.exists(path + ".html")) {
      request->send(LittleFS, path + ".html", "text/html");
      return;
    }
    snprintf(logbuf, LOGBUF_SIZE, "404 %s %s", request->client()->remoteIP().toString().c_str(), path.c_str());
    log::toAll(logbuf);
    request->send(404);
  });

  startAppWebServer();
}

#endif
