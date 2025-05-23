#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <RTClib.h>
#include <Servo.h>

#define ONE_WIRE_BUS 2
#define FAN_PIN 11
#define PH_PIN A7
#define SAMPLES 10
#define PH_SLOPE -8.041885
#define PH_OFFSET 30.701568
#define PH_LED_PIN 10
#define SERVO_PIN 3
#define FEED_BUTTON_PIN 5

LiquidCrystal_I2C lcd(0x27, 16, 2);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
RTC_DS3231 rtc;
Servo feederServo;

bool fanRunning = false;
bool phAlertActive = false;

// Manual feeding variables
volatile bool manualFeedRequested = false;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 500;

// Auto feeding interval (every 2 minutes)
unsigned long lastAutoFeedTime;
const unsigned long autoFeedInterval = 60000; // 1 minutes in milliseconds

// Servo positions for feeding
const int VALVE_CLOSED_POSITION = 0;
const int VALVE_OPEN_POSITION = 90;
const int FEEDING_DURATION = 1000;

// Interrupt service routine for manual feed button
void buttonInterrupt()
{
  if ((millis() - lastDebounceTime) > debounceDelay)
  {
    manualFeedRequested = true;
    lastDebounceTime = millis();
  }
}

void setup()
{
  Serial.begin(9600);
  analogReference(EXTERNAL);
  lcd.init();
  lcd.backlight();

  sensors.begin();
  sensors.setResolution(12);

  if (!rtc.begin())
  {
    Serial.println("RTC Error");
    lcd.setCursor(0, 0);
    lcd.print("RTC Error!");
    while (1)
      ;
  }

  // Uncomment to sync time from computer
  // rtc.adjust(DateTime(F(_DATE), F(TIME_)));

  pinMode(FAN_PIN, OUTPUT);
  pinMode(PH_LED_PIN, OUTPUT);
  pinMode(FEED_BUTTON_PIN, INPUT_PULLUP);

  // Set up interrupt for manual feed button on pin D5
  attachInterrupt(digitalPinToInterrupt(FEED_BUTTON_PIN), buttonInterrupt, FALLING);

  digitalWrite(FAN_PIN, LOW);
  digitalWrite(PH_LED_PIN, LOW);

  feederServo.attach(SERVO_PIN);
  feederServo.write(VALVE_CLOSED_POSITION);

  lastAutoFeedTime = millis(); // Initialize feeding timer

  lcd.setCursor(0, 0);
  lcd.print("pH:      ");
  lcd.setCursor(0, 1);
  lcd.print("Temp:");

  // Initial display for feed status
  lcd.setCursor(11, 1);
  lcd.print("READY");
}

float readPH()
{
  long sum = 0;
  for (int i = 0; i < SAMPLES; i++)
  {
    sum += analogRead(PH_PIN);
    delay(10);
  }

  float voltage = (float)sum / SAMPLES * 5.0 / 1024.0;
  float pH = (voltage * PH_SLOPE) + PH_OFFSET;

  return pH;
}

void feedFish()
{
  lcd.setCursor(0, 0);
  lcd.print("FEEDING...      ");
  feederServo.write(VALVE_OPEN_POSITION);
  Serial.println("Feeding: Valve opened");
  delay(FEEDING_DURATION);
  feederServo.write(VALVE_CLOSED_POSITION);
  Serial.println("Feeding: Valve closed");
  lcd.setCursor(0, 0);
  lcd.print("pH:      ");
}

void loop()
{
  DateTime now = rtc.now();

  // Display time
  lcd.setCursor(11, 0);
  if (now.hour() < 10)
    lcd.print("0");
  lcd.print(now.hour());
  lcd.print(":");
  if (now.minute() < 10)
    lcd.print("0");
  lcd.print(now.minute());

  // Check if manual feeding was requested via the button interrupt
  if (manualFeedRequested)
  {
    Serial.println("Manual feeding requested");
    lcd.setCursor(11, 1);
    lcd.print("MANUAL");
    feedFish();
    manualFeedRequested = false;
    // Reset the auto feed timer after manual feeding
    lastAutoFeedTime = millis();
    // Display ready status after feeding
    delay(1000);
    lcd.setCursor(11, 1);
    lcd.print("READY ");
  }

  // Auto feeding every 2 minutes
  unsigned long currentMillis = millis();
  if (currentMillis - lastAutoFeedTime >= autoFeedInterval)
  {
    Serial.println("Auto feeding (2-minute interval)");
    lcd.setCursor(11, 1);
    lcd.print("AUTO  ");
    feedFish();
    lastAutoFeedTime = currentMillis;
    // Display ready status after feeding
    delay(1000);
    lcd.setCursor(11, 1);
    lcd.print("READY ");
  }

  // Read and display pH
  float pH = readPH();
  lcd.setCursor(3, 0);
  lcd.print("     ");
  lcd.setCursor(3, 0);
  lcd.print(pH, 2);

  // Read and display temperature
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);

  if (tempC != DEVICE_DISCONNECTED_C)
  {
    lcd.setCursor(5, 1);
    lcd.print("     ");
    lcd.setCursor(5, 1);
    lcd.print(tempC, 1);
    lcd.print("C");

    // Control fans based on temperature
    if (tempC >= 30.0 && !fanRunning)
    {
      digitalWrite(FAN_PIN, HIGH);
      fanRunning = true;
      Serial.println("Fans ON");
      lcd.setCursor(11, 1);
      if (!manualFeedRequested)
      {
        lcd.print("FAN:ON");
      }
    }
    else if (tempC <= 28.0 && fanRunning)
    {
      digitalWrite(FAN_PIN, LOW);
      fanRunning = false;
      Serial.println("Fans OFF");
      lcd.setCursor(11, 1);
      if (!manualFeedRequested)
      {
        lcd.print("READY ");
      }
    }
  }
  else
  {
    lcd.setCursor(5, 1);
    lcd.print("ERROR!");
    Serial.println("Temp read error");
  }

  // Print status to serial monitor
  Serial.print("pH: ");
  Serial.print(pH, 2);
  Serial.print(" | Temp: ");
  Serial.print(tempC);
  Serial.print("C | Fan: ");
  Serial.print(fanRunning ? "ON" : "OFF");
  Serial.print(" | pH Alert: ");
  Serial.println(phAlertActive ? "ON" : "OFF");

  delay(750);
}