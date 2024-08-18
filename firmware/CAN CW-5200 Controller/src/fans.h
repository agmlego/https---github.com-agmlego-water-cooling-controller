#ifndef __CW5200_FANS__
#define __CW5200_FANS__
#include <cstdint>

const float HZ_TO_RPM = 30.0;

struct struct_fan
{
    bool ready = false;
    uint32_t pulse_len = 0;
    uint32_t last_pulse = 0;
    uint32_t current_pulse = 0;
};

float convertMicrosToRPM(float);
uint8_t turnOnFans(uint8_t pin, uint8_t pwm = 255);
uint8_t turnOffFans(uint8_t pin, uint8_t pwm = 0);
#endif