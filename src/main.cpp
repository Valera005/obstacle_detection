#include <Arduino.h>
#include <LiquidCrystal.h>
#include <EEPROM.h>
#include <IRremote.h>

#include "../lib/helpers/helpers.h"
#include "../lib/IRRemoteMap/IRremoteMap.h"

#define BUTTON_PIN 2

#define GREEN_LED_PIN 10
#define YELLOW_LED_PIN 11
#define RED_LED_PIN 12

#define LCD_RS_PIN A5
#define LCD_E_PIN A4
#define LCD_D4_PIN 6
#define LCD_D5_PIN 7
#define LCD_D6_PIN 8
#define LCD_D7_PIN 9

#define IR_RECEIVE_PIN 5

#define ECHO_PIN 3
#define TRIGGER_PIN 4

#define PHOTORESISTOR_PIN A0
#define MIN_PHOTORESISTOR 150
#define MAX_PHOTORESISTOR 950
#define PHOTORESISTOR_RANGE 800

LiquidCrystal lcd(LCD_RS_PIN, LCD_E_PIN, LCD_D4_PIN,
                  LCD_D5_PIN, LCD_D6_PIN, LCD_D7_PIN);

constexpr int LED_ARRAY_SIZE = 3;
int LED_ARRAY[LED_ARRAY_SIZE] = {10, 11, 12};

unsigned long current_time = 0;

// Measure Distance
volatile unsigned long current_time_pulse = 0;
volatile unsigned long startTimeDistanceDetection = 0, pulseDuration = 200, lastTimePulseDuration = 200;
volatile bool isMeasured = false;
bool isCm = true;

unsigned long lastTimeMeasureDistance = 0, measureDistanceDelay = 200;

// Yellow led blinking
unsigned long yellowLEDDelay = 100;
unsigned long lastTimeYellowLED = 0;

// Blocking logic
bool isBlocked = 0;
unsigned int blockingLogicDelay = 200;
unsigned long blockedLogicLastTime = 0;

// IR reciever
unsigned long IRDelay = 400;
unsigned long lastTimeIR = 0;

// State of game
int state = 0;
String line_1 = "", line_2 = "";

// displaying message on LCD Logic
unsigned int LCDDelay = 50;
unsigned long LCDLastTime = 0;

// luminosity
unsigned int luminosity = 0;

void triggerDistanceDetection()
{
  // in case signal was already high
  digitalWrite(TRIGGER_PIN, LOW);
  delayMicroseconds(2);

  // triggering sensor for at least 10mcs
  digitalWrite(TRIGGER_PIN, HIGH);
  delayMicroseconds(10);

  //
  digitalWrite(TRIGGER_PIN, LOW);
}

void measureDistance()
{
  current_time_pulse = micros();
  if (digitalRead(ECHO_PIN) == HIGH)
  {
    startTimeDistanceDetection = current_time_pulse;
  }
  else
  {
    pulseDuration = current_time_pulse - startTimeDistanceDetection;
    isMeasured = true;
  }
}

double getDistanceInCm()
{
  return pulseDuration / 58.0;
}

double getDistanceInInches()
{
  return pulseDuration / 58.0 * 2;
}

String getStringDistance()
{
  if (isCm)
    return String(getDistanceInCm()) + " cm";

  else
    return String(getDistanceInInches()) + " in";
}

double getPercentageOfLight(double photoValue)
{
  return (photoValue - MIN_PHOTORESISTOR) / PHOTORESISTOR_RANGE;
}

void powerOffAllLED()
{
  for (int led : LED_ARRAY)
  {
    digitalWrite(led, LOW);
  }
}

void toggleLED(int pin)
{
  digitalWrite(pin, !digitalRead(pin));
}

void printUserTextOnDisplay(String text)
{
  for (int i = text.length(); i < 16; i++)
  {
    text += " ";
  }
  lcd.print(text);
}

double getPercentageOfLight()
{
  return (double(luminosity) - MIN_PHOTORESISTOR) / double(PHOTORESISTOR_RANGE);
}

void setup()
{
  Serial.begin(115200);
  Serial.setTimeout(50);

  lcd.begin(16, 2);

  printUserTextOnDisplay("Initialising");

  IrReceiver.begin(IR_RECEIVE_PIN, true);

  isCm = EEPROM.read(0);

  pinMode(ECHO_PIN, INPUT);
  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);

  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(YELLOW_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(ECHO_PIN), measureDistance, CHANGE);
}

void loop()
{
  current_time = millis();

  luminosity = analogRead(A0);
  Serial.println(getPercentageOfLight());
  analogWrite(GREEN_LED_PIN, (1 - getPercentageOfLight()) * 255);

  if (!isBlocked && current_time - lastTimeMeasureDistance >= measureDistanceDelay)
  {
    lastTimeMeasureDistance += measureDistanceDelay;
    triggerDistanceDetection();
  }

  if (!isBlocked && isMeasured)
  {
    double distanceCm = getDistanceInCm();
    yellowLEDDelay = distanceCm;

    line_1 = "Dist: " + getStringDistance();

    if (distanceCm < 20)
    {
      isBlocked = true;
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(YELLOW_LED_PIN, LOW);

      line_1 = "!!! Obstacle !!!";
      line_2 = "Press to unlock";
    }
    else if (distanceCm > 20 && distanceCm < 70)
    {
      line_2 = "WARGNING!!!";
    }
    else
    {
      line_2 = "ALL GOOD";
    }

    if (state == 0)
    {
      lcd.setCursor(0, 0);
      printUserTextOnDisplay(line_1);

      lcd.setCursor(0, 1);
      printUserTextOnDisplay(line_2);
    }
  }

  if (isBlocked && digitalRead(BUTTON_PIN))
  {
    isBlocked = false;
    lcd.clear();
  }

  if (!isBlocked && current_time - lastTimeYellowLED >= yellowLEDDelay)
  {
    toggleLED(YELLOW_LED_PIN);
    lastTimeYellowLED += yellowLEDDelay;
  }

  if (isBlocked && current_time - blockedLogicLastTime >= blockingLogicDelay)
  {
    toggleLED(YELLOW_LED_PIN);
    toggleLED(RED_LED_PIN);
    blockedLogicLastTime += blockingLogicDelay;
  }

  if (IrReceiver.decode() && current_time - lastTimeIR >= IRDelay)
  {
    int command = IrReceiver.decodedIRData.command;
    switch (command)
    {
    case IR_BUTTON_PLAYSTOP:
      if (isBlocked)
      {
        isBlocked = false;
        lcd.clear();
      }
      break;

    case IR_BUTTON_EQ:
      isCm = !isCm;
      EEPROM.write(0, isCm);
      break;

    case IR_BUTTON_UP:
      state = (state + 1) % 3;
      break;

    case IR_BUTTON_DOWN:
      if (state == 0)
        state = 2;
      else
        state--;
      break;

    case IR_BUTTON_POWER:
      if (state == 2)
      {
        state = 0;
        isCm = true;
        EEPROM.write(0, isCm);
      }
      break;

    default:
      break;
    }

    lastTimeIR += IRDelay;
    IrReceiver.resume();
  }

  if (current_time - LCDLastTime >= LCDDelay)
  {
    if (state == 1)
    {
      lcd.clear();
      lcd.setCursor(0, 0);
      printUserTextOnDisplay("Lumin: " + String(luminosity));
    }
    else if (state == 2)
    {
      lcd.clear();
      lcd.setCursor(0, 0);
      printUserTextOnDisplay("Press on OFF to");
      lcd.setCursor(0, 1);
      printUserTextOnDisplay("reset settings");
    }

    LCDLastTime += LCDDelay;
  }
}
