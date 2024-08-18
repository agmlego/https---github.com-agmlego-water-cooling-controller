#ifndef __CW5200_SETTINGS__
#define __CW5200_SETTINGS__
#include <cstdint>
#include <EEPROM.h>

struct struct_settings
{
    uint8_t version;
    uint16_t filter_high_limit;
    uint16_t filter_zero;
    uint8_t case_temperature_high_limit;
    uint8_t case_temperature_low_limit;
    uint8_t case_humidity_high_limit;

    uint16_t reservoir_volume_low_limit;
    uint16_t reservoir_ref_zero;
    uint8_t reservoir_temp_high_limit;
    uint8_t reservoir_temp_low_limit;
    uint8_t outside_temp_high_limit;
    uint8_t outside_temp_low_limit;

    uint32_t valve_lockout;
    uint32_t compressor_lockout;

    float hysteresis;
};

void saveSettings(struct_settings);

struct_settings *loadSettings();

#endif