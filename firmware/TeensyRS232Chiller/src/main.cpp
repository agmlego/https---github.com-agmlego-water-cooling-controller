#include <Arduino.h>
#include <RunningAverage.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE 2   // DS18B20 bus
#define RES_LEVEL A9 // Analog input for eTape Rsense
#define RES_REF A8   // Analog input for eTape Rref

RunningAverage resLvlRA(100);
RunningAverage resRefRA(100);

#define TEMPERATURE_PRECISION 9
OneWire oneWire(ONE_WIRE);
DallasTemperature sensors(&oneWire);
DeviceAddress outside_temp = {0x28, 0x4E, 0x6A, 0x45, 0x92, 0x17, 0x02, 0xEC};
DeviceAddress reservoir_temp = {0x28, 0xFF, 0x02, 0x5D, 0xC1, 0x17, 0x05, 0xCB};
float reservoir_temp_reading = 0.0;
float outside_temp_reading = 0.0;

char msg;
bool pause;
uint32_t timeout = 0;
uint8_t samples = 0;
uint8_t retries = 0;

void setup(void)
{
  Serial.begin(9600);
  while (!Serial && millis() < 5000)
  {
    // wait up to 5 seconds for Arduino Serial Monitor
  }
  Serial.println("Chiller-side RS-232 Test");

  // explicitly start clean
  resLvlRA.clear();
  resRefRA.clear();

  sensors.begin();
  if (!sensors.getAddress(outside_temp, 0))
  {
    Serial.println("Error: Unable to find address for outside_temp");
  }
  else
  {
    sensors.setResolution(outside_temp, TEMPERATURE_PRECISION);
  }

  if (!sensors.getAddress(reservoir_temp, 1))
  {
    Serial.println("Error: Unable to find address for reservoir_temp");
  }
  else
  {
    sensors.setResolution(reservoir_temp, TEMPERATURE_PRECISION);
  }
  pause = true;
}

void loop()
{
  if (!pause)
  { /*
     *   Reservoir Temp Measurement
     */
    sensors.requestTemperatures();
    reservoir_temp_reading = sensors.getTempC(reservoir_temp);
    if (reservoir_temp_reading == DEVICE_DISCONNECTED_C)
    {
      retries = 0;
      while (++retries <= 3)
      {
        sensors.requestTemperaturesByAddress(reservoir_temp);
        reservoir_temp_reading = sensors.getTempC(reservoir_temp);
        if (reservoir_temp_reading == DEVICE_DISCONNECTED_C)
        {
          break;
        }
      }
      if (retries >= 3)
      {
        Serial.println("Error: Could not read reservoir temperature data");
        reservoir_temp_reading = 0.0;
      }
    }
    else
    {
      retries = 0;
    }

    /*
     *   Outside Temp Measurement
     */
    outside_temp_reading = sensors.getTempC(outside_temp);
    if (outside_temp_reading == DEVICE_DISCONNECTED_C)
    {
      retries = 0;
      while (++retries <= 3)
      {
        sensors.requestTemperaturesByAddress(outside_temp);
        outside_temp_reading = sensors.getTempC(outside_temp);
        if (outside_temp_reading == DEVICE_DISCONNECTED_C)
        {
          break;
        }
      }
      if (retries >= 3)
      {
        Serial.println("Error: Could not read outside temperature data");
        outside_temp_reading = 0.0;
      }
    }
    else
    {
      retries = 0;
    }
    if (millis() - timeout >= 10)
    {
      timeout = millis();
      resLvlRA.addValue(analogRead(RES_LEVEL));
      resRefRA.addValue(analogRead(RES_REF));
      ++samples;
    }
    if (samples >= 100)
    {
      Serial.printf("%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
                    resLvlRA.getAverage(),
                    resLvlRA.getStandardDeviation(),
                    resRefRA.getAverage(),
                    resRefRA.getStandardDeviation(),
                    outside_temp_reading,
                    reservoir_temp_reading);
      samples = 0;
    }
  }

  if (Serial.available() > 0)
  {
    msg = Serial.read();
    if (msg == 'r')
    {
      Serial.println("Reset!");
      resLvlRA.clear();
      resRefRA.clear();
      timeout = 0;
      samples = 0;
    }
    if (msg == 'p')
    {
      Serial.println("Pause!");
      pause = true;
    }
    if (msg == 's')
    {
      Serial.println("Start!");
      pause = false;
    }
  }
}