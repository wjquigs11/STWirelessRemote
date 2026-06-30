#ifdef NTP
#include "include.h"

bool ntpSyncSuccessful = false;
static TimerHandle_t ntpCheckTimer = NULL;
static int ntpRetryCount = 0;
#define NTP_MAX_RETRIES 20  // 20 * 500ms = 10 seconds

// Flag for ntpCheckTask to poll
static bool ntpCheckPending = false;

void setupTime() {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
    setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1); // Pacific Time
    tzset();
}

// Task for NTP sync checking - runs with 4096 bytes stack to handle logging
static void ntpCheckTask(void* param) {
    for (;;) {
        if (ntpCheckPending) {
            ntpCheckPending = false;
            
            time_t now = time(nullptr);
            ntpRetryCount++;

            if (now > 1577836800) {
                // Sync successful
                struct tm timeinfo;
                localtime_r(&now, &timeinfo);
                char buf[50];
                strftime(buf, sizeof(buf), "%A, %B %d %Y %H:%M:%S", &timeinfo);
                snprintf(logbuf, LOGBUF_SIZE, "NTP sync successful: %s", buf);
                log::toAll(logbuf);
                ntpSyncSuccessful = true;
                xTimerDelete(ntpCheckTimer, 0);
                ntpCheckTimer = NULL;
            } else if (ntpRetryCount >= NTP_MAX_RETRIES) {
                log::toAll("NTP sync failed after timeout");
                ntpSyncSuccessful = false;
                xTimerDelete(ntpCheckTimer, 0);
                ntpCheckTimer = NULL;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Check every 100ms
    }
}

static void ntpCheckCallback(TimerHandle_t xTimer) {
    // Just set the flag - the actual work is done in ntpCheckTask
    // This keeps the timer callback minimal and avoids timer daemon stack overflow
    ntpCheckPending = true;
}

void waitForTimeSync(int timeoutSeconds) {
    log::toAll("Starting async NTP sync...");
    ntpRetryCount = 0;
    ntpSyncSuccessful = false;

    if (ntpCheckTimer != NULL) {
        xTimerDelete(ntpCheckTimer, 0);
    }
    // Check every 500ms, up to timeoutSeconds
    ntpCheckTimer = xTimerCreate("ntpCheck", 500 / portTICK_PERIOD_MS, pdTRUE, NULL, ntpCheckCallback);
    if (ntpCheckTimer) {
        xTimerStart(ntpCheckTimer, 0);
        // Start the ntpCheckTask to handle sync checks
        xTaskCreate(ntpCheckTask, "ntpCheck", 4096, NULL, 1, NULL);
        log::toAll("NTP check task started");
    } else {
        log::toAll("Failed to create NTP timer");
    }
}

String getFormattedTime() {
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char buf[50];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(buf);
}

unsigned long getEpochTime() {
    time_t now;
    time(&now);
    return now;
}

bool isNtpSyncSuccessful() {
    return ntpSyncSuccessful;
}

void resyncNTP() {
    log::toAll("Resyncing NTP time...");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
    setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
    tzset();
    waitForTimeSync(10);
}
#endif