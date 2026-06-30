
#ifndef LOGTO_H
#define LOGTO_H

#define LOGBUF_SIZE 256
extern char logbuf[LOGBUF_SIZE];

#define LOG_RING_SIZE 32
#define LOG_LINE_MAX 128

class log {
public:
    static bool logToSerial;
    log() {}
    static void toAll(const char* s);
    static void flush();  // call from loop() to write buffered lines to file
    static bool initConsole(String conslogName = "/console.log");
};
#endif
