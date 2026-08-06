#ifndef SIGNALMANAGER_H
#define SIGNALMANAGER_H

#ifdef NMEA
#include "Nmea.h"
#endif
#include "Models/SeaTalkData.h"

class SignalManager
{

public:
    SignalManager(SeaTalkData *seaTalkData);
    void UpdateApparentWindAngle(double angle);
    void UpdateApparentWindSpeed(double speed);
    void UpdateSpeedThroughWater(double speed);
    void UpdateSpeedOverGround(double speed);
    void UpdateCourseOverGround(double courseOverGround);
    void UpdateCompassHeading(double heading);

private:
#ifdef NMEA
    Nmea *_nmea;
#endif
    SeaTalkData *_seaTalkData;
};

#endif
