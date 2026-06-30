#include "SignalManager.h"

SignalManager::SignalManager(SeaTalkData *seaTalkData)
{
    _seaTalkData = seaTalkData;
#ifdef NMEA
    _nmea = new Nmea();
#endif
}

void SignalManager::UpdateApparentWindAngle(double windAngle)
{
    _seaTalkData->apparentWindAngle = windAngle;
#ifdef NMEA
    _nmea->updateApparentWindAngle(windAngle);
#endif
}

void SignalManager::UpdateApparentWindSpeed(double windSpeed)
{
    _seaTalkData->apparentWindSpeed = windSpeed;
#ifdef NMEA
    _nmea->updateApparentWindSpeed(windSpeed);
#endif
}

void SignalManager::UpdateSpeedThroughWater(double speed)
{
    _seaTalkData->speedThroughWater = speed;
#ifdef NMEA
    _nmea->updateSTW(speed);
#endif
}

void SignalManager::UpdateSpeedOverGround(double speed)
{
    _seaTalkData->speedOverGround = speed;
#ifdef NMEA
    _nmea->updateSOG(speed);
#endif
}

void SignalManager::UpdateCourseOverGround(double courseOverGround)
{
    _seaTalkData->courseOverGround = courseOverGround;
#ifdef NMEA
    _nmea->updateCOG(courseOverGround);
#endif
}
