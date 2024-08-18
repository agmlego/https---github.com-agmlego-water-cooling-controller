#include "settings.h"

static struct_settings settings = {
    .version = 3,
    .filter_high_limit = 500,
    .filter_zero = 100,
    .case_temperature_high_limit = 100,
    .case_temperature_low_limit = 0,
    .case_humidity_high_limit = 80,
    .reservoir_volume_low_limit = 0,
    .reservoir_ref_zero = 620,
    .reservoir_temp_high_limit = 30,
    .reservoir_temp_low_limit = 5,
    .outside_temp_high_limit = 100,
    .outside_temp_low_limit = 0,
    .valve_lockout = 30 * 1000,
    .compressor_lockout = 60 * 1000,
    .hysteresis = 2.0,
};

void saveSettings(struct_settings *new_settings)
{
    EEPROM.put(0, *new_settings);
}

struct_settings *loadSettings()
{
    if (EEPROM.read(0) != settings.version)
    {
        saveSettings(&settings);
    }
    else
    {
        EEPROM.get(0, settings);
    }
    return &settings;
}