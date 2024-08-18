#include <Arduino.h>
#include "fans.h"

float convertMicrosToRPM(float micros)
{
    return HZ_TO_RPM * 1000000.0 / micros;
}

uint8_t turnOnFans(uint8_t pin, uint8_t pwm)
{
    analogWrite(pin, pwm);
    return pwm;
}

uint8_t turnOffFans(uint8_t pin, uint8_t pwm)
{
    analogWrite(pin, pwm);
    return pwm;
}