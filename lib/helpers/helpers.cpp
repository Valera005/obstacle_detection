#include <Arduino.h>
#include <helpers.h>

bool checkIfIntInSerial()
{
  char nextChar = Serial.peek(); // allows to get next char in serial
  // communication without deleting it from buffer

  return nextChar >= '0' && nextChar <= '9';
}

int analogToPWM(int analog)
{
  return double(analog) / 4;
}