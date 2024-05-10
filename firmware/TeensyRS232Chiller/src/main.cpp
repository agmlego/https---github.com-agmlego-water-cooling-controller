#include <Arduino.h>

String inMsg;
uint32_t timeout = 0;

void setup(void)
{
  Serial.begin(9600);
  while (!Serial && millis() < 5000)
  {
    // wait up to 5 seconds for Arduino Serial Monitor
  }
  Serial.println("Chiller-side RS-232 Test");

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial1.begin(9600);
}

void loop()
{
  if (Serial1.available() > 0)
  {
    inMsg = Serial1.readStringUntil((char)'\x03');
    Serial.printf("%lu - Received message: \n\t<- %s\n", millis(), inMsg.c_str());
    Serial1.print(inMsg.c_str());
    digitalWrite(LED_BUILTIN, LOW);
  }
  if (millis() - timeout >= 100)
  {
    timeout = millis();
    digitalWrite(LED_BUILTIN, HIGH);
  }
}