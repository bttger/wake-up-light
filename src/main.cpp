#include <Arduino.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

/**
 * --- Type definitions ---
 */

struct SunriseConfig
{
  int hour;
  int minute;
  int duration;
  int utcOffset;
};

/**
 * --- Function prototypes ---
 */

// Print the date and time to the serial monitor
void printDateTime(const RtcDateTime &dt);

// Print the sunrise config to the serial monitor
void printSunriseConfig(SunriseConfig config);

// Set the RTC date and time from Unix epoch time
void setDateTimeFromUnixEpoch(uint32_t epoch);

// Save a sunrise configuration into RTC memory
void saveSunriseConfig(SunriseConfig config);

// Retrieve the sunrise config from the RTC memory
SunriseConfig getSunriseConfig();

// Connect with the AP (try for 30 seconds), then fetch
// the sunrise config from the API, set it in the RTC
// memory, and update the RTC time by fetching the current
// time from the time API. After that, turn off the
// WiFi again.
void updateBoardState();

/**
 * --- Constants ---
 */
#define DELAY_TIME 2000
#define SSID "sunrise"
#define PASSWORD "sunrise1"
#define WIFI_ATTEMPT_TIME_SECS 10
// JSON response from the API:
// {
//   "sunrise_hour": 7,
//   "sunrise_minute": 0,
//   "duration_min": 60,
//   "utc_offset": 0
// }
#define SUNRISE_API_URL "https://raw.githubusercontent.com/bttger/wake-up-light/main/sunrise.json"
// JSON response from the API (only unixtime key is relevant):
// {
//   "unixtime": 1700261973,
//   ...
// }
#define TIME_API_URL "http://worldtimeapi.org/api/timezone/Europe/London"
#define UPDATE_BOARD_STATE 0
#define IO_PIN_LED 5
#define PWM_CHANNEL 0      // 0-15
#define PWM_FREQUENCY 5000 // 5 kHz
#define PWM_RESOLUTION 8   // 8-bit resolution (0-255)

/**
 * --- Global variables ---
 */
ThreeWire myWire(11, 12, 10); // DAT/IO, CLK, RST/CE/CS pin connections
RtcDS1302<ThreeWire> Rtc(myWire);
SunriseConfig config;

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

  // Initialize the sunrise config from the RTC memory
  config = getSunriseConfig();

  // Initialize PWM
  ledcSetup(PWM_CHANNEL, PWM_FREQUENCY, PWM_RESOLUTION);
  ledcAttachPin(IO_PIN_LED, PWM_CHANNEL);

  if (UPDATE_BOARD_STATE)
    updateBoardState();
}

void loop()
{
  RtcDateTime now = Rtc.GetDateTime();
  printDateTime(now);
  Serial.println();
  printSunriseConfig(config);

  // DEBUG: Go through 5 different brightness levels
  for (int i = 0; i <= 5; i++)
  {
    Serial.print("Setting brightness to ");
    Serial.println(51 * i);
    ledcWrite(PWM_CHANNEL, 51 * i);
    delay(DELAY_TIME);
  }
}

/**
 * --- Function definitions ---
 */

void updateBoardState()
{
  // Connect to Wi-Fi
  WiFi.begin(SSID, PASSWORD);
  Serial.println("Connecting to WiFi...");
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_ATTEMPT_TIME_SECS * 1000)
  {
    delay(1000);
    Serial.print(".");
  }
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Failed to connect to WiFi.");
    return;
  }
  Serial.println("Connected to WiFi");

  // Create a WiFiClientSecure object
  WiFiClientSecure secureClient;
  secureClient.setInsecure(); // Allow connection without certificate

  HTTPClient http;

  // Fetch sunrise config
  http.begin(secureClient, SUNRISE_API_URL);
  int httpResponseCode = http.GET();
  if (httpResponseCode > 0)
  {
    String payload = http.getString();
    StaticJsonDocument<200> doc;
    deserializeJson(doc, payload);
    saveSunriseConfig({doc["sunrise_hour"], doc["sunrise_minute"], doc["duration_min"], doc["utc_offset"]});
    Serial.println("Sunrise configuration updated");
  }
  else
  {
    Serial.print("Error on fetching sunrise data: ");
    Serial.println(httpResponseCode);
  }
  http.end(); // Free resources

  // Fetch current Unix time using non-secure http request
  http.begin(TIME_API_URL);
  httpResponseCode = http.GET();
  if (httpResponseCode > 0)
  {
    String payload = http.getString();
    StaticJsonDocument<500> doc;
    deserializeJson(doc, payload);
    setDateTimeFromUnixEpoch(doc["unixtime"]);
    Serial.println("RTC time updated");
  }
  else
  {
    Serial.print("Error on fetching time data: ");
    Serial.println(httpResponseCode);
  }
  http.end(); // Free resources

  // Disconnect Wi-Fi
  WiFi.disconnect(true);
  Serial.println("Disconnected from WiFi");
}

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

void printSunriseConfig(SunriseConfig config)
{
  Serial.print("Sunrise config: ");
  Serial.print(config.hour);
  Serial.print(":");
  Serial.print(config.minute);
  Serial.print(" (");
  Serial.print(config.duration);
  Serial.print(" min, UTC");
  Serial.print(config.utcOffset);
  Serial.println(")");
}

void setDateTimeFromUnixEpoch(uint32_t epoch)
{
  RtcDateTime dt;
  dt.InitWithUnix32Time(epoch);
  Rtc.SetDateTime(dt);
}

void saveSunriseConfig(SunriseConfig config)
{
  Rtc.SetMemory((uint8_t)0, (uint8_t)config.hour);
  Rtc.SetMemory((uint8_t)1, (uint8_t)config.minute);
  Rtc.SetMemory((uint8_t)2, (uint8_t)config.duration);
  Rtc.SetMemory((uint8_t)3, (uint8_t)config.utcOffset);
}

SunriseConfig getSunriseConfig()
{
  // Retrieve hour and minute from RTC memory
  int hour = Rtc.GetMemory(0);
  int minute = Rtc.GetMemory(1);
  int duration = Rtc.GetMemory(2);
  int utcOffset = Rtc.GetMemory(3);

  // If values are invalid, return a default time of 07:00
  if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || duration < 0 || duration > 120 || utcOffset < -12 || utcOffset > 12)
  {
    Serial.println("Invalid sunrise config on RTC memory, using default");
    return {7, 0, 60, 1};
  }

  return {hour, minute, duration, utcOffset};
}
