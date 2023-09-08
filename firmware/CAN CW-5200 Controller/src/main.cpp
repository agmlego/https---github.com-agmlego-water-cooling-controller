#include <Arduino.h>
#include <FanController.h>
#include <Adafruit_BME280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Adafruit_VL53L0X.h"
#include "RunningAverage.h"
#include <OneWire.h>
#include <DallasTemperature.h>

#define TOP_FAN_RPM 0     // INPUT Top fan tach
#define BOTTOM_FAN_RPM 1  // INPUT Bottom fan tach
#define BOTTOM_FAN_PWM 5  // OUTPUT fan PWM signal
#define FLOW_SW 8         // INPUT PULLUP flow switch; low == OK
#define PUMP_RLY 9        // OUTPUT pump
#define VALVE_RLY 10      // OUTPUT valve
#define COMPRESSOR_RLY 11 // OUTPUT compressor
#define ALARMS_RLY 12     // OUTPUT alarms
#define ONE_WIRE 2        // DS18B20 bus
#define I2C_SDA 18        // Local I2C data
#define I2C_SCL 19        // Local I2C clock
#define FILTER_P A0       // Analog input for differential pressure sensor
RunningAverage filterRA(100);

#define SENSOR_THRESHOLD 1000
FanController top_fan(TOP_FAN_RPM, SENSOR_THRESHOLD);
FanController bottom_fan(BOTTOM_FAN_RPM, SENSOR_THRESHOLD, BOTTOM_FAN_PWM);
unsigned int top_fan_reading = 0;
unsigned int bottom_fan_reading = 0;

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

uint32_t reading_time = 0;
uint8_t reading_state = 0;
bool led_state = 1;

Adafruit_VL53L0X lox = Adafruit_VL53L0X();
RunningAverage resRA(100);
const double reservoir_area = 0.031416; // Liters per mm height
const double reservoir_height = 270.0;  // total reservoir height
double reservoir_volume = 0.0;
#define RESERVOIR_LOW_ALARM 5.8

#define TEMPERATURE_PRECISION 9
OneWire oneWire(ONE_WIRE);
DallasTemperature sensors(&oneWire);
DeviceAddress reservoir_temp = {0x28, 0xFF, 0x02, 0x5D, 0xC1, 0x17, 0x05, 0xCB};
DeviceAddress outside_temp = {0x28, 0x4E, 0x6A, 0x45, 0x92, 0x17, 0x02, 0xEC};
float reservoir_temp_reading = 0.0;
float outside_temp_reading = 0.0;
float reservoir_setpoint = 20.0;
float hysteresis = 2.0;
unsigned long last_compressor = 0;
unsigned long last_valve = 0;
unsigned long compressor_time = 0;
unsigned long valve_time = 0;
const unsigned long valve_lockout = 30 * 1000;
const unsigned long compressor_lockout = 60 * 1000;

int ringMeter(const char *, int, int, int, int, int, int, const char *);

void setup()
{
    resRA.clear(); // explicitly start clean
    filterRA.clear();
    pinMode(FLOW_SW, INPUT_PULLUP);
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(PUMP_RLY, OUTPUT);
    pinMode(VALVE_RLY, OUTPUT);
    pinMode(COMPRESSOR_RLY, OUTPUT);
    pinMode(ALARMS_RLY, OUTPUT);
    digitalWrite(PUMP_RLY, HIGH);
    digitalWrite(VALVE_RLY, LOW);
    digitalWrite(COMPRESSOR_RLY, LOW);
    digitalWrite(ALARMS_RLY, LOW);

    top_fan.begin();
    bottom_fan.begin();
    bottom_fan.setDutyCycle(0);

    Serial.begin(9600);
    while (!Serial && millis() < 5000)
    {
        // wait up to 5 seconds for Arduino Serial Monitor
    }
    Serial.println("CAN CW_5200 Controller");

    while (!bme.begin(BME_ADDRESS))
    {
        Serial.println("No connect to BME!");
        digitalWrite(LED_BUILTIN, HIGH);
        delay(100);
        digitalWrite(LED_BUILTIN, LOW);
        delay(100);
    }
    bme_temp->printSensorDetails();
    bme_humidity->printSensorDetails();

    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    while (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
    {
        Serial.println("No connect to display!");
        digitalWrite(LED_BUILTIN, HIGH);
        delay(250);
        digitalWrite(LED_BUILTIN, LOW);
        delay(250);
    }
    display.setTextSize(1);              // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE); // Draw white text
    display.setCursor(0, 0);             // Start at top-left corner
    display.cp437(true);                 // Use full 256 char 'Code Page 437' font

    while (!lox.begin())
    {
        Serial.println(F("Failed to boot VL53L0X"));
        digitalWrite(LED_BUILTIN, HIGH);
        delay(500);
        digitalWrite(LED_BUILTIN, LOW);
        delay(100);
    }
    lox.startRangeContinuous();

    sensors.begin();
    if (!oneWire.search(reservoir_temp))
        Serial.println("Unable to find address for reservoir_temp");
    if (!oneWire.search(outside_temp))
        Serial.println("Unable to find address for outside_temp");

    sensors.setResolution(reservoir_temp, TEMPERATURE_PRECISION);
    sensors.setResolution(outside_temp, TEMPERATURE_PRECISION);
}

void loop()
{
    if (lox.isRangeComplete())
    {
        reservoir_volume = (reservoir_height - lox.readRange()) * reservoir_area;
        resRA.addValue(reservoir_volume);
    }
    digitalWrite(LED_BUILTIN, LOW);
    sensors_event_t temp_event, humidity_event;
    bme_temp->getEvent(&temp_event);
    bme_humidity->getEvent(&humidity_event);
    top_fan_reading = top_fan.getSpeed();
    bottom_fan_reading = bottom_fan.getSpeed();
    filterRA.addValue(analogRead(FILTER_P));
    sensors.requestTemperatures();
    reservoir_temp_reading = sensors.getTempC(reservoir_temp);
    if (reservoir_temp_reading == DEVICE_DISCONNECTED_C)
    {
        Serial.println("Error: Could not read reservoir temperature data");
        reservoir_temp_reading = 0.0;
    }
    outside_temp_reading = sensors.getTempC(outside_temp);
    if (outside_temp_reading == DEVICE_DISCONNECTED_C)
    {
        Serial.println("Error: Could not read outside temperature data");
        outside_temp_reading = 0.0;
    }

    compressor_time = millis() - last_compressor;
    valve_time = millis() - last_valve;

    if (reservoir_temp_reading > reservoir_setpoint + hysteresis)
    {
        if (last_valve == 0)
        {
            last_valve = millis();
            digitalWrite(VALVE_RLY, HIGH);
        }
        if (valve_time >= valve_lockout && compressor_time >= compressor_lockout)
        {
            last_compressor = millis();
            bottom_fan.setDutyCycle(100);
            digitalWrite(COMPRESSOR_RLY, HIGH);
        }
    }
    if (reservoir_temp_reading <= reservoir_setpoint - hysteresis)
    {
        if (millis() - last_compressor >= compressor_lockout)
        {
            last_compressor = millis();
            last_valve = millis();
            bottom_fan.setDutyCycle(0);
            digitalWrite(COMPRESSOR_RLY, LOW);
            digitalWrite(VALVE_RLY, LOW);
        }
    }

    Serial.printf("Volume: %.1fL\tDelta P: %.1f\tFlow SW: %d\tRes Temp: %.1fC\tSetpoint: %.1fC+/-%.1fC\tLast valve: %lums\tLockout %lums\tLast comp.: %lums\tLockout %lums\n",
                  resRA.getAverage(),
                  filterRA.getAverage(),
                  digitalRead(FLOW_SW),
                  reservoir_temp_reading,
                  reservoir_setpoint,
                  hysteresis,
                  valve_time,
                  valve_lockout,
                  compressor_time,
                  compressor_lockout);

    if (millis() - reading_time >= PAGE_DELAY)
    {
        reading_time = millis();
        ++reading_state;
        if (reading_state >= 4)
            reading_state = 0;
        switch (reading_state)
        {
        case 0:
            display.clearDisplay();
            ringMeter("Case T", temp_event.temperature, 0, 100, 0, 0, GAUGE_RADIUS, "\xF8"
                                                                                    "C");
            ringMeter("Case RH", humidity_event.relative_humidity, 0, 100, SCREEN_WIDTH - 2 * GAUGE_RADIUS, 0, GAUGE_RADIUS, "%");
            display.display();
            break;
        case 1:
            display.clearDisplay();
            ringMeter("Res T", reservoir_temp_reading, 0, 100, 0, 0, GAUGE_RADIUS, "\xF8"
                                                                                   "C");
            ringMeter("Out T", outside_temp_reading, 0, 100, SCREEN_WIDTH - 2 * GAUGE_RADIUS, 0, GAUGE_RADIUS, "\xF8"
                                                                                                               "C");
            display.display();
            break;
        case 2:
            display.clearDisplay();
            ringMeter("Top Fan", top_fan_reading, 0, 3200, 0, 0, GAUGE_RADIUS, "RPM");
            ringMeter("Bot Fan", bottom_fan_reading, 0, 3200, SCREEN_WIDTH - 2 * GAUGE_RADIUS, 0, GAUGE_RADIUS, "RPM");
            display.display();
            break;
        case 3:
            display.clearDisplay();
            ringMeter("Res Lvl", (int)(resRA.getAverage()), 0, (int)(reservoir_area * reservoir_height), 0, 0, GAUGE_RADIUS, "L");
            ringMeter("\x83 P", (int)(filterRA.getAverage()), 0, 1024, SCREEN_WIDTH - 2 * GAUGE_RADIUS, 0, GAUGE_RADIUS, "ADC");
            display.display();
            break;
        default:
            break;
        }
    }
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