#include "fans.h"

const float HZ_TO_RPM = 30.0;

float convertMicrosToRPM(float micros)
{
    return HZ_TO_RPM * 1000000.0 / micros;
}