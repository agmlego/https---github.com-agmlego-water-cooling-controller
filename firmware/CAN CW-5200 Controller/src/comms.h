#ifndef __CW5200_COMMS__
#define __CW5200_COMMS__

struct __attribute__((packed)) struct_readings
{
    struct __attribute__((packed))
    {
        float temperature;
        float setpoint;
        float level_sense;
        float level_ref;
    } reservoir;
    struct __attribute__((packed))
    {
        float inside_temperature;
        float outside_temperature;
        float humidity;
        uint16_t filter_dp;
        struct __attribute__((packed))
        {
            float top_tach;
            float bottom_tach;
            uint8_t pwm;
        } fan;
    } chassis;
    struct __attribute__((packed))
    {
        bool running;
        bool valve;
        uint32_t compressor_time;
        uint32_t valve_time;
    } compressor;
    struct __attribute__((packed))
    {
        bool running;
        bool flow_ok;
    } pump;
    struct __attribute__((packed))
    {
        bool alert;
        uint16_t code;
    } error;
};

#endif