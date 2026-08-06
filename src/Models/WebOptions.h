#ifndef WEBOPTIONS_H
#define WEBOPTIONS_H

#include <Arduino.h>

#define WIND_SERVER_DEFAULT_HOST "espwind.local"

struct WebOptions
{
    int button1;
    int button2;
    int button3;
    int button4;
    int button5;
    int button6;
    int button7;
    int button8;
    int timermin;
    int timersec;
    String windhost;
    bool seatalkDebugRx;   // debug for received messages (default true)
    bool seatalkDebugTx;  // debug for transmitted messages (default false)
    int headingOffset;     // degrees to add to compass heading before N2K transmit
    bool xmitHeading;      // transmit heading on N2K bus
};

#endif
