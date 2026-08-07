#include "Options.h"

Options::Options()
{
    _preferences.begin("NautiControl", false);
}

void Options::SaveWebOptions(WebOptions webOptions)
{
    _preferences.putInt("button1", webOptions.button1);
    _preferences.putInt("button2", webOptions.button2);
    _preferences.putInt("button3", webOptions.button3);
    _preferences.putInt("button4", webOptions.button4);
    _preferences.putInt("button5", webOptions.button5);
    _preferences.putInt("button6", webOptions.button6);
    _preferences.putInt("button7", webOptions.button7);
    _preferences.putInt("button8", webOptions.button8);
    _preferences.putInt("timermin", webOptions.timermin);
    _preferences.putInt("timersec", webOptions.timersec);
    _preferences.putString("windhost", webOptions.windhost);
    _preferences.putBool("seatalkDebugRx", webOptions.seatalkDebugRx);
    _preferences.putBool("seatalkDebugTx", webOptions.seatalkDebugTx);
    _preferences.putInt("headingOffset", webOptions.headingOffset);
    _preferences.putBool("xmitHeading", webOptions.xmitHeading);
    _preferences.putFloat("windXmitHz", webOptions.windXmitHz);
}

WebOptions Options::GetWebOptions()
{
    WebOptions webOptions;
    webOptions.button1 = _preferences.getInt("button1", 0);
    webOptions.button2 = _preferences.getInt("button2", 1);
    webOptions.button3 = _preferences.getInt("button3", 2);
    webOptions.button4 = _preferences.getInt("button4", 3);
    webOptions.button5 = _preferences.getInt("button5", 4);
    webOptions.button6 = _preferences.getInt("button6", 5);
    webOptions.button7 = _preferences.getInt("button7", 6);
    webOptions.button8 = _preferences.getInt("button8", 7);
    webOptions.timermin = _preferences.getInt("timermin", 5);
    webOptions.timersec = _preferences.getInt("timersec", 0);
    webOptions.windhost = _preferences.getString("windhost", WIND_SERVER_DEFAULT_HOST);
    webOptions.seatalkDebugRx = _preferences.getBool("seatalkDebugRx", true);
    webOptions.seatalkDebugTx = _preferences.getBool("seatalkDebugTx", false);
    webOptions.headingOffset = _preferences.getInt("headingOffset", 0);
    webOptions.xmitHeading = _preferences.getBool("xmitHeading", false);
    webOptions.windXmitHz = _preferences.getFloat("windXmitHz", 1.0f);
    return webOptions;
}
