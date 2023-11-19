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
  int durationMinutes;
  int keepLightOnMinutes;
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

// Debug function to test the LED PWM. The LED should
// fade in and out indefinitely with a 2 second pause
// at the bottom.
void debugLedPwm();

// Start the sunrise sequence with the given config
void startSunrise(int durationMins, int keepOnForMins);

/**
 * --- Constants ---
 */
#define UPDATE_BOARD_STATE 1
#define SSID "sunrise"
#define PASSWORD "sunrise1"
#define WIFI_ATTEMPT_TIME_SECS 5
// JSON response from the API:
// {
//   "sunriseHour": 7,
//   "sunriseMinute": 0,
//   "durationMinutes": 60,
//   "keepLightOnMinutes": 30,
//   "utcOffset": 0
// }
#define SUNRISE_API_URL "https://raw.githubusercontent.com/bttger/wake-up-light/main/sunrise.json"
// JSON response from the API (only unixtime key is relevant):
// {
//   "unixtime": 1700261973,
//   ...
// }
#define TIME_API_URL "http://worldtimeapi.org/api/timezone/Europe/London"
#define WAIT_FOR_SERIAL_OUTPUT 0
#define DEBUG_INFO 0
#define DEBUG_LED_PWM 0
#define IO_PIN_LED 5
#define PWM_CHANNEL 0 // 0-15
#define PWM_FREQUENCY 5000
#define PWM_RESOLUTION 12
#define PWM_MAX_DUTY_CYCLE pow(2, PWM_RESOLUTION) - 1

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
#if WAIT_FOR_SERIAL_OUTPUT
  while (!Serial)
    delay(1000);
#endif

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

#if DEBUG_INFO
  RtcDateTime now = Rtc.GetDateTime();
  printDateTime(now);
  Serial.println();
  printSunriseConfig(config);
#endif

  // Initialize PWM
  ledcSetup(PWM_CHANNEL, PWM_FREQUENCY, PWM_RESOLUTION);
  ledcAttachPin(IO_PIN_LED, PWM_CHANNEL);

#if DEBUG_LED_PWM
  startSunrise(2, 0);
  debugLedPwm();
#endif

#if UPDATE_BOARD_STATE
  updateBoardState();
#endif
}

void loop()
{
  RtcDateTime now = Rtc.GetDateTime();
  if (now.Hour() + config.utcOffset == config.hour && now.Minute() == config.minute)
  {
    Serial.print("Starting sunrise sequence at ");
    printDateTime(now);
    startSunrise(config.durationMinutes, config.keepLightOnMinutes);
  }
  delay(5000);
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
    StaticJsonDocument<300> doc;
    deserializeJson(doc, payload);
    saveSunriseConfig({doc["sunriseHour"], doc["sunriseMinute"], doc["durationMinutes"], doc["keepLightOnMinutes"], doc["utcOffset"]});
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
  Serial.print(config.durationMinutes);
  Serial.print(" mins, ");
  Serial.print(config.keepLightOnMinutes);
  Serial.print(" mins, UTC");
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
  Rtc.SetMemory((uint8_t)2, (uint8_t)config.durationMinutes);
  Rtc.SetMemory((uint8_t)3, (uint8_t)config.keepLightOnMinutes);
  Rtc.SetMemory((uint8_t)4, (uint8_t)config.utcOffset);
}

SunriseConfig getSunriseConfig()
{
  // Retrieve hour and minute from RTC memory
  int hour = Rtc.GetMemory(0);
  int minute = Rtc.GetMemory(1);
  int durationMinutes = Rtc.GetMemory(2);
  int keepLightOnMinutes = Rtc.GetMemory(3);
  int utcOffset = Rtc.GetMemory(4);

  // If values are invalid, return a default time of 07:00
  if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || durationMinutes < 0 || durationMinutes > 120 || keepLightOnMinutes < 0 || keepLightOnMinutes > 120 || utcOffset < -12 || utcOffset > 12)
  {
    Serial.println("Invalid sunrise config on RTC memory, using default");
    return {7, 0, 60, 30, 1};
  }

  return {hour, minute, durationMinutes, keepLightOnMinutes, utcOffset};
}

void debugLedPwm()
{
  int reverse = 0;
  int dutyCycle = 0;
  while (1)
  {
    dutyCycle += reverse ? -1 : 1;
    ledcWrite(PWM_CHANNEL, dutyCycle);
    delay(10);
    if (dutyCycle == PWM_MAX_DUTY_CYCLE)
      reverse = 1;
    if (dutyCycle == 1)
      delay(2000);
    if (dutyCycle == 0)
    {
      reverse = 0;
      delay(2000);
    }
  }
}

void startSunrise(int durationMins, int keepOnForMins)
{
  int durationMillis = durationMins * 60000;
  int startMillis = millis();
  float exponent = 1.8;

  while (1)
  {
    int currentMillis = millis();
    int elapsedMillis = currentMillis - startMillis;

    if (elapsedMillis >= durationMillis)
    {
      // Sunrise is over, keep the LED on for some time
      delay(keepOnForMins * 60000);
      ledcWrite(PWM_CHANNEL, 0);
      break;
    }

    // Calculate the exponential duty cycle
    float progress = (float)elapsedMillis / (float)durationMillis;
    float exponentialProgress = pow(progress, exponent);
    int dutyCycle = (int)(exponentialProgress * PWM_MAX_DUTY_CYCLE);

    ledcWrite(PWM_CHANNEL, dutyCycle);
    delay(20);
  }
}
