#include <Arduino.h>
#include <FreqMeasureMulti.h>
#include <Adafruit_BME280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RunningAverage.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SerialTransfer.h>

#include "error_codes.h"
#include "settings.h"
#include "comms.h"
#include "fans.h"

#define SER_RX 0          // Serial Rx
#define SER_TX 1          // Serial Tx
#define ONE_WIRE 2        // DS18B20 bus
#define VALVE_RLY 8       // OUTPUT valve
#define COMPRESSOR_RLY 9  // OUTPUT compressor
#define FAN_PWM 10        // OUTPUT fan PWM signal
#define BOTTOM_FAN_RPM 11 // INPUT Bottom fan tach
#define TOP_FAN_RPM 12    // INPUT Top fan tach
#define ALARMS_RLY 13     // OUTPUT alarms
#define PUMP_RLY 14       // OUTPUT pump
#define FLOW_SW 15        // INPUT PULLUP flow switch; low == OK
#define I2C_SDA 17        // Local I2C data
#define I2C_SCL 16        // Local I2C clock
#define ENCODER_SWITCH 18 // Encoder push switch
#define ENCODER_A 20      // Encoder quad channel A
#define ENCODER_B 19      // Encoder quad channel B
#define FILTER_P A7       // Analog input for differential pressure sensor
#define RES_LEVEL A9      // Analog input for eTape Rsense
#define RES_REF A8        // Analog input for eTape Rref

struct_readings readings;

RunningAverage filterRA(100);

#define FAN_SAMPLING_TIME 1000
volatile struct_fan top_fan;
volatile struct_fan bottom_fan;
RunningAverage topRA(10);
RunningAverage bottomRA(10);

#define BME_ADDRESS 0x76
Adafruit_BME280 bme; // use I2C interface
Adafruit_Sensor *bme_temp = bme.getTemperatureSensor();
Adafruit_Sensor *bme_humidity = bme.getHumiditySensor();

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define FONT_X 5
#define FONT_Y 8
#define GAUGE_RADIUS 28
#define PAGE_DELAY 5000
#define OLED_RESET -1       // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

RunningAverage resLvlRA(100);
RunningAverage resRefRA(100);

#define TEMPERATURE_PRECISION 9
OneWire oneWire(ONE_WIRE);
DallasTemperature sensors(&oneWire);
DeviceAddress outside_temp = {0x28, 0x4E, 0x6A, 0x45, 0x92, 0x17, 0x02, 0xEC};
DeviceAddress reservoir_temp = {0x28, 0xFF, 0x02, 0x5D, 0xC1, 0x17, 0x05, 0xCB};

uint32_t last_compressor = 0;
uint32_t last_valve = 0;

bool running_state = true;
uint32_t reading_time = 0;
uint8_t reading_state = 0;

struct_settings *settings;

SerialTransfer telemetry;
uint16_t txSize = 0;

void measureReservoirLevel();
void measureChassisTempHumid();
void measureReservoirTemp();
void measureOutsideTemp();
void measureFanRPM();
void measureFilterDP();
void runCoolingCycle();
void setError(uint16_t);
void printAddress(DeviceAddress);
void updateDisplay();
void top_fan_pulse();
void bottom_fan_pulse();
int ringMeter(const char *, int, int, int, int, int, int, const char *);

void setup()
{
    // get settings from EEPROM
    settings = loadSettings();

    // explicitly start clean
    resLvlRA.clear();
    resRefRA.clear();
    filterRA.clear();

    // switch I2C to alternate pins
    Wire.setSDA(I2C_SDA);
    Wire.setSCL(I2C_SCL);

    pinMode(PUMP_RLY, OUTPUT);
    digitalWrite(PUMP_RLY, LOW);
    pinMode(FLOW_SW, INPUT_PULLUP);
    pinMode(VALVE_RLY, OUTPUT);
    digitalWrite(VALVE_RLY, HIGH);
    pinMode(COMPRESSOR_RLY, OUTPUT);
    digitalWrite(COMPRESSOR_RLY, LOW);
    pinMode(ALARMS_RLY, OUTPUT);
    digitalWrite(ALARMS_RLY, LOW);

    pinMode(TOP_FAN_RPM, INPUT);
    attachInterrupt(TOP_FAN_RPM, top_fan_pulse, FALLING);
    pinMode(BOTTOM_FAN_RPM, INPUT);
    attachInterrupt(BOTTOM_FAN_RPM, bottom_fan_pulse, FALLING);
    topRA.clear();
    bottomRA.clear();
    pinMode(FAN_PWM, OUTPUT);
    analogWriteFrequency(FAN_PWM, 25000);
    readings.chassis.fan.pwm = 0;
    analogWrite(FAN_PWM, readings.chassis.fan.pwm);

    readings.reservoir.setpoint = 20.0;

    Serial.begin(9600);
    while (!Serial && millis() < 5000)
    {
        // wait up to 5 seconds for Arduino Serial Monitor
    }
    Serial.println("RS232 CW_5200 Controller");

    if (!bme.begin(BME_ADDRESS))
    {
        setError(CASE_BME280_NO_CONNECT);
        Serial.printf("Error %04X: No connect to BME!\n", readings.error.code);
    }
    else
    {
        bme_temp->printSensorDetails();
        bme_humidity->printSensorDetails();
    }

    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
    {
        setError(CASE_DISPLAY_NO_CONNECT);
        Serial.printf("Error %04X: No connect to display!\n", readings.error.code);
    }
    else
    {
        display.setTextSize(1);              // Normal 1:1 pixel scale
        display.setTextColor(SSD1306_WHITE); // Draw white text
        display.setCursor(0, 0);             // Start at top-left corner
        display.cp437(true);                 // Use full 256 char 'Code Page 437' font
    }

    sensors.begin();
    if (!sensors.getAddress(outside_temp, 0))
    {
        setError(CASE_NO_OUTSIDE_DS18B20_ADDRESS);
        Serial.printf("Error %04X: Unable to find address for outside_temp\n", readings.error.code);
    }
    else
    {
        Serial.print("outside_temp Address: ");
        printAddress(outside_temp);
        Serial.println();
        sensors.setResolution(outside_temp, TEMPERATURE_PRECISION);
    }

    if (!sensors.getAddress(reservoir_temp, 1))
    {
        setError(RESERVOIR_NO_DS18B20_ADDRESS);
        Serial.printf("Error %04X: Unable to find address for reservoir_temp\n", readings.error.code);
    }
    else
    {
        Serial.print("reservoir_temp Address: ");
        printAddress(reservoir_temp);
        Serial.println();
        sensors.setResolution(reservoir_temp, TEMPERATURE_PRECISION);
    }

    /*
     *  Set up telemetry link
     */
    Serial1.begin(19200);
    telemetry.begin(Serial1);
    delay(5000);
}

void loop()
{
    measureReservoirLevel();
    measureChassisTempHumid();
    measureReservoirTemp();
    measureOutsideTemp();
    measureFanRPM();
    measureFilterDP();
    runCoolingCycle();

    // send telemetry
    txSize = 0;
    txSize = telemetry.txObj(readings, txSize);
    telemetry.sendData(txSize);

    updateDisplay();
    delay(1000); // TODO: REMOVE ME
}

void measureReservoirLevel()
{
    /*
     *   Reservoir Level Measurement
     */
    resLvlRA.addValue(analogRead(RES_LEVEL));
    resRefRA.addValue(analogRead(RES_REF));
    readings.reservoir.level_sense = resLvlRA.getAverage();
    readings.reservoir.level_ref = resRefRA.getAverage();
    if (readings.reservoir.level_sense < settings->reservoir_volume_low_limit)
    {
        setError(RESERVOIR_LEVEL_LOW);
        Serial.printf("Error %04X: Reservoir level too low! %dmL < %dmL\n", readings.error.code, (int)readings.reservoir.level_sense, settings->reservoir_volume_low_limit);
    }
}

void measureChassisTempHumid()
{
    /*
     *   Case Temp and RH Measurement
     */
    sensors_event_t temp_event, humidity_event;
    bme_temp->getEvent(&temp_event);
    bme_humidity->getEvent(&humidity_event);
    readings.chassis.inside_temperature = temp_event.temperature;
    readings.chassis.humidity = humidity_event.relative_humidity;
    if (readings.chassis.inside_temperature > settings->case_temperature_high_limit)
    {
        setError(CASE_TEMP_TOO_HIGH);
        Serial.printf("Error %04X: Case temperature too high! %dC > %dC\n", readings.error.code, readings.chassis.inside_temperature, settings->case_temperature_high_limit);
    }
    if (readings.chassis.inside_temperature < settings->case_temperature_low_limit)
    {
        setError(CASE_TEMP_TOO_LOW);
        Serial.printf("Error %04X: Case temperature too low! %dC < %dC\n", readings.error.code, readings.chassis.inside_temperature, settings->case_temperature_low_limit);
    }
    if (readings.chassis.humidity > settings->case_humidity_high_limit)
    {
        setError(CASE_HUMIDITY_TOO_HIGH);
        Serial.printf("Error %04X: Case humidity too high! %d%% > %d%%\n", readings.error.code, readings.chassis.humidity, settings->case_humidity_high_limit);
    }
}

void measureReservoirTemp()
{
    /*
     *   Reservoir Temp Measurement
     */
    sensors.requestTemperaturesByAddress(reservoir_temp);
    readings.reservoir.temperature = sensors.getTempC(reservoir_temp);
    if (readings.reservoir.temperature == DEVICE_DISCONNECTED_C)
    {
        setError(RESERVOIR_NO_DS18B20_READ);
        Serial.printf("Error %04X: Could not read reservoir temperature data\n", readings.error.code);
        readings.reservoir.temperature = 0.0;
    }
    if (readings.reservoir.temperature > settings->reservoir_temp_high_limit)
    {
        setError(RESERVOIR_TEMP_TOO_HIGH);
        Serial.printf("Error %04X: Reservoir temperature too high! %dC > %dC\n", readings.error.code, readings.reservoir.temperature, settings->reservoir_temp_high_limit);
    }
    if (readings.reservoir.temperature < settings->reservoir_temp_low_limit)
    {
        setError(RESERVOIR_TEMP_TOO_LOW);
        Serial.printf("Error %04X: Reservoir temperature too low! %dC < %dC\n", readings.error.code, readings.reservoir.temperature, settings->reservoir_temp_low_limit);
    }
}

void measureOutsideTemp()
{
    /*
     *   Outside Temp Measurement
     */
    sensors.requestTemperaturesByAddress(outside_temp);
    readings.chassis.outside_temperature = sensors.getTempC(outside_temp);
    if (readings.chassis.outside_temperature == DEVICE_DISCONNECTED_C)
    {
        setError(CASE_NO_OUTSIDE_DS18B20_READ);
        Serial.printf("Error %04X: Could not read outside temperature data\n", readings.error.code);
        readings.chassis.outside_temperature = 0.0;
    }
    if (readings.chassis.outside_temperature > settings->outside_temp_high_limit)
    {
        setError(CASE_OUTSIDE_TEMP_TOO_HIGH);
        Serial.printf("Error %04X: Outside temperature too high! %dC > %dC\n", readings.error.code, readings.chassis.outside_temperature, settings->outside_temp_high_limit);
    }
    if (readings.chassis.outside_temperature < settings->outside_temp_low_limit)
    {
        setError(CASE_OUTSIDE_TEMP_TOO_LOW);
        Serial.printf("Error %04X: Outside temperature too low! %dC < %dC\n", readings.error.code, readings.chassis.outside_temperature, settings->outside_temp_low_limit);
    }
}

void measureFilterDP()
{
    /*
     *   Filter Delta-P Measurement
     */
    filterRA.addValue(analogRead(FILTER_P));
    readings.chassis.filter_dp = filterRA.getAverage();
    if (filterRA.getAverage() > settings->filter_high_limit)
    {
        setError(CASE_FILTERS_CLOGGED);
        Serial.printf("Error %04X: Filter delta-P too high! %d > %d\n", readings.error.code, (int)readings.chassis.filter_dp, settings->filter_high_limit);
    }
}

void measureFanRPM()
{
    /*
     *   Fan RPM Measurement
     */
    if (top_fan.ready)
    {
        topRA.addValue(top_fan.pulse_len);
        top_fan.ready = false;
    }
    if (bottom_fan.ready)
    {
        bottomRA.addValue(bottom_fan.pulse_len);
        bottom_fan.ready = false;
    }
    if (millis() - reading_time > FAN_SAMPLING_TIME)
    {
        if (topRA.getCount() > 0)
        {
            readings.chassis.fan.top_tach = convertMicrosToRPM(topRA.getAverage());
        }
        else
        {
            readings.chassis.fan.top_tach = 0;
            if (readings.chassis.fan.pwm > 0)
            {
                setError(CASE_TOP_FAN_LOW_RPM);
                Serial.printf("Error %04X: Top fan RPM too low!\n", readings.error.code);
            }
        }

        if (bottomRA.getCount() > 0)
        {
            readings.chassis.fan.bottom_tach = convertMicrosToRPM(bottomRA.getAverage());
        }
        else
        {
            readings.chassis.fan.bottom_tach = 0;
            if (readings.chassis.fan.pwm > 0)
            {
                setError(CASE_BOTTOM_FAN_LOW_RPM);
                Serial.printf("Error %04X: Bottom fan RPM too low!\n", readings.error.code);
            }
        }
    }
}

void runCoolingCycle()
{
    /*
     *   Cooling Cycle
     */
    readings.compressor.compressor_time = millis() - last_compressor;
    readings.compressor.valve_time = millis() - last_valve;
    readings.pump.flow_ok = (digitalRead(FLOW_SW) == LOW);
    readings.pump.running = (digitalRead(PUMP_RLY) == LOW);

    if (running_state)
    {
        if (readings.reservoir.temperature > readings.reservoir.setpoint + settings->hysteresis)
        {
            if (digitalRead(VALVE_RLY) == HIGH)
            {
                last_valve = millis();
                digitalWrite(VALVE_RLY, LOW);
                readings.compressor.valve = true;
            }
            if (readings.compressor.valve_time >= settings->valve_lockout && readings.compressor.compressor_time >= settings->compressor_lockout)
            {
                last_compressor = millis();
                readings.chassis.fan.pwm = 255;
                analogWrite(FAN_PWM, readings.chassis.fan.pwm);
                digitalWrite(COMPRESSOR_RLY, HIGH);
                readings.compressor.running = true;
            }
        }
        if (readings.reservoir.temperature <= readings.reservoir.setpoint - settings->hysteresis)
        {
            if (millis() - last_compressor >= settings->compressor_lockout)
            {
                last_compressor = millis();
                last_valve = millis();
                readings.chassis.fan.pwm = 0;
                analogWrite(FAN_PWM, readings.chassis.fan.pwm);
                digitalWrite(COMPRESSOR_RLY, LOW);
                digitalWrite(VALVE_RLY, HIGH);
                readings.compressor.running = false;
                readings.compressor.valve = false;
            }
        }
        if (readings.pump.running && !readings.pump.flow_ok)
        {
            setError(RESERVOIR_PUMP_ON_WITH_NO_FLOW);
            Serial.printf("Error %04X: No flow with pump running!\n", readings.error.code);
        }
    }
}

void setError(uint16_t error)
{
    readings.error.code = error;
    readings.error.alert = true;
    digitalWrite(ALARMS_RLY, LOW);
}

// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        // zero pad the address if necessary
        if (deviceAddress[i] < 16)
            Serial.print("0");
        Serial.print(deviceAddress[i], HEX);
    }
}

void updateDisplay()
{
    /*
     *   Display Update Cycle
     */
    if (millis() - reading_time >= PAGE_DELAY)
    {
        reading_time = millis();
        ++reading_state;
        if (reading_state >= 6)
            reading_state = 0;
        switch (reading_state)
        {
        case 0:
            display.clearDisplay();
            ringMeter("Case T", readings.chassis.inside_temperature, 0, 100, 0, 0, GAUGE_RADIUS, "\xF8"
                                                                                                 "C");
            ringMeter("Case RH", readings.chassis.humidity, 0, 100, SCREEN_WIDTH - 2 * GAUGE_RADIUS, 0, GAUGE_RADIUS, "%");
            display.display();
            break;
        case 1:
            display.clearDisplay();
            ringMeter("Res T", readings.reservoir.temperature, 0, 100, 0, 0, GAUGE_RADIUS, "\xF8"
                                                                                           "C");
            ringMeter("Out T", readings.chassis.outside_temperature, 0, 100, SCREEN_WIDTH - 2 * GAUGE_RADIUS, 0, GAUGE_RADIUS, "\xF8"
                                                                                                                               "C");
            display.display();
            break;
        case 2:
            display.clearDisplay();
            ringMeter("Top Fan", readings.chassis.fan.top_tach, 0, 6000, 0, 0, GAUGE_RADIUS, "RPM");
            ringMeter("Bot Fan", readings.chassis.fan.bottom_tach, 0, 6000, SCREEN_WIDTH - 2 * GAUGE_RADIUS, 0, GAUGE_RADIUS, "RPM");
            display.display();
            break;
        case 3:
            display.clearDisplay();
            ringMeter("Res Lvl", (int)readings.reservoir.level_sense, 0, 1024, 0, 0, GAUGE_RADIUS, "ADC");
            ringMeter("Res Ref", (int)readings.reservoir.level_ref, 0, 1024, SCREEN_WIDTH - 2 * GAUGE_RADIUS, 0, GAUGE_RADIUS, "ADC");
            display.display();
            break;
        case 4:
            display.clearDisplay();
            ringMeter("Res Set", readings.reservoir.setpoint, 10, 30, 0, 0, GAUGE_RADIUS, "\xF8"
                                                                                          "C");
            ringMeter("\x83 P", (int)readings.chassis.filter_dp, 0, 1024, SCREEN_WIDTH - 2 * GAUGE_RADIUS, 0, GAUGE_RADIUS, "ADC");
            display.display();
            break;
        case 5:
            display.clearDisplay();
            ringMeter("Comp", readings.compressor.compressor_time / 1000, 0, (2 * settings->compressor_lockout) / 1000, 0, 0, GAUGE_RADIUS, "s");
            ringMeter("Valve", readings.compressor.valve_time / 1000, 0, (2 * settings->valve_lockout) / 1000, SCREEN_WIDTH - 2 * GAUGE_RADIUS, 0, GAUGE_RADIUS, "s");
            display.display();
            break;
        default:
            break;
        }
    }
}

void top_fan_pulse()
{
    top_fan.current_pulse = micros();
    top_fan.pulse_len = top_fan.current_pulse - top_fan.last_pulse;
    top_fan.last_pulse = top_fan.current_pulse;
    top_fan.ready = true;
}

void bottom_fan_pulse()
{
    bottom_fan.current_pulse = micros();
    bottom_fan.pulse_len = bottom_fan.current_pulse - bottom_fan.last_pulse;
    bottom_fan.last_pulse = bottom_fan.current_pulse;
    bottom_fan.ready = true;
}

// #########################################################################
//  Draw the meter on the screen, returns x coord of righthand side
// #########################################################################
int ringMeter(const char *reading, int value, int vmin, int vmax, int orig_x, int orig_y, int r, const char *units)
{
    int x = orig_x + r;
    int y = orig_y + r + FONT_Y; // Calculate coords of centre of ring

    int w = r / 4; // Width of outer ring is 1/4 of radius

    int angle = 150; // Half the sweep angle of meter (300 degrees)

    int v = map(value, vmin, vmax, -angle, angle); // Map the value to an angle v

    byte seg = 5; // Segments are 5 degrees wide = 60 segments for 300 degrees
    byte inc = 5; // Draw segments every 5 degrees, increase to 10 for segmented ring

    // Draw colour blocks every inc degrees
    for (int i = -angle; i < angle; i += inc)
    {
        // Calculate pair of coordinates for segment start
        float sx = cos((i - 90) * 0.0174532925);
        float sy = sin((i - 90) * 0.0174532925);
        uint16_t x0 = sx * (r - w) + x;
        uint16_t y0 = sy * (r - w) + y;
        uint16_t x1 = sx * r + x;
        uint16_t y1 = sy * r + y;

        // Calculate pair of coordinates for segment end
        float sx2 = cos((i + seg - 90) * 0.0174532925);
        float sy2 = sin((i + seg - 90) * 0.0174532925);
        int x2 = sx2 * (r - w) + x;
        int y2 = sy2 * (r - w) + y;
        int x3 = sx2 * r + x;
        int y3 = sy2 * r + y;

        if (i < v)
        { // Fill in coloured segments with 2 triangles
            display.fillTriangle(x0, y0, x1, y1, x2, y2, SSD1306_WHITE);
            display.fillTriangle(x1, y1, x2, y2, x3, y3, SSD1306_WHITE);
        }
        else // Fill in blank segments
        {
            display.fillTriangle(x0, y0, x1, y1, x2, y2, SSD1306_BLACK);
            display.fillTriangle(x1, y1, x2, y2, x3, y3, SSD1306_BLACK);
        }
    }

    // Convert value to a string
    char buf[10];
    byte len = 1;
    if (value > 9)
        len = 2;
    if (value > 99)
        len = 3;
    if (value > 999)
        len = 4;
    dtostrf(value, len, 0, buf);

    // Set the text colour to default
    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);

    // Print reading
    display.setTextSize(1);
    display.setCursor(x - ((FONT_X * strlen(reading)) / 2), orig_y);
    display.print(reading);

    // Print value
    display.setTextSize(2);
    display.setCursor(x - (FONT_X * 2 * len / 2), y - FONT_Y); // Value in middle
    display.print(buf);

    // Print units

    display.setTextSize(1);
    display.setCursor(x - ((FONT_X * strlen(units)) / 2), y + FONT_Y); // Units display
    display.print(units);

    // Calculate and return right hand side x coordinate
    return x + r;
}