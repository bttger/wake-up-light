#include <Arduino.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>

/**
 * --- Function prototypes ---
 */

// Function to print the date and time
void printDateTime(const RtcDateTime &dt);

// Function to set the RTC date and time from Unix epoch time
void setDateTimeFromEpoch(uint32_t epoch);

// Function to save a specific time (e.g., sunrise time) into RTC memory
void saveSunriseTime(int hour, int minute);

// Function to retrieve the sunrise time
RtcDateTime getSunriseTime();

/**
 * --- Constants ---
 */
#define DELAY_TIME 2000

/**
 * --- Global variables ---
 */
ThreeWire myWire(11, 12, 10); // DAT/IO, CLK, RST/CE/CS pin connections
RtcDS1302<ThreeWire> Rtc(myWire);

/**
 * --- Setup and loop ---
 */
void setup()
{
  Serial.begin(9600);
  while (!Serial)
    delay(DELAY_TIME); // time to get serial running
  Serial.println("Wake-Up-Light");

  Rtc.Begin();

  // Check if the RTC is write protected, disable write protection if it is
  if (Rtc.GetIsWriteProtected())
  {
    Serial.println("RTC was write protected, enabling writing now");
    Rtc.SetIsWriteProtected(false);
  }

  // Check if the RTC is running, set the time if it's not
  if (!Rtc.GetIsRunning())
  {
    Serial.println("RTC is not running; starting and setting the time...");
    Rtc.SetIsRunning(true);
    RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
    Rtc.SetDateTime(compiled);
  }
}

void loop()
{
  RtcDateTime now = Rtc.GetDateTime();
  printDateTime(now);
  Serial.println();
  delay(DELAY_TIME);
}

/**
 * --- Function definitions ---
 */

void printDateTime(const RtcDateTime &dt)
{
  char datestring[20];

  snprintf_P(datestring,
             countof(datestring),
             PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
             dt.Day(),
             dt.Month(),
             dt.Year(),
             dt.Hour(),
             dt.Minute(),
             dt.Second());
  Serial.print(datestring);
}

void setDateTimeFromEpoch(uint32_t epoch)
{
  RtcDateTime dt = RtcDateTime(epoch);
  Rtc.SetDateTime(dt);
}

void saveSunriseTime(int hour, int minute)
{
  Rtc.SetMemory((uint8_t)0, (uint8_t)hour);
  Rtc.SetMemory((uint8_t)1, (uint8_t)minute);
}

RtcDateTime getSunriseTime()
{
  // Retrieve hour and minute from RTC memory
  int hour = Rtc.GetMemory(0);
  int minute = Rtc.GetMemory(1);

  // If values are invalid, return a default time of 07:00
  if (hour < 0 || hour > 23 || minute < 0 || minute > 59)
  {
    return RtcDateTime(0, 1, 1, 7, 0, 0);
  }

  return RtcDateTime(0, 1, 1, hour, minute, 0);
}
