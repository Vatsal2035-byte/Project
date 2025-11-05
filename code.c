#include <TM1637Display.h>
#include <math.h>

#define CLK PB6 // TM1637 CLK pin
#define DIO PB7 // TM1637 DIO pin
#define ANALOG_PIN PA0 // Voltage divider junction

TM1637Display display(CLK, DIO);

const float VREF = 3.3; // Reference voltage
const float R_FIXED = 10000; // Fixed resistor in ohms
const float BETA = 3950; // Beta constant for NTC103
const float T0 = 298.15; // 25°C in Kelvin
const float R0 = 10000; // Resistance of NTC at 25°C

void setup() {
  display.setBrightness(5);
}

void loop() {
  int adcValue = analogRead(ANALOG_PIN); // 0–4095 (12-bit ADC)
  float voltage = (adcValue * VREF) / 4095.0;
  
  // Calculate thermistor resistance
  float R_therm = R_FIXED * (VREF / voltage - 1.0);

  // Avoid division by zero if voltage = VREF
  if (R_therm <= 0) return;

  // Apply Beta equation
  float temperatureK = 1.0 / ((1.0 / T0) + (1.0 / BETA) * log(R_therm / R0));
  float temperatureC = temperatureK - 273.15;

  // Convert for display (e.g., 25.6°C → 25)
  int displayValue = (int)(temperatureC +27+ 0.5);

  // Display temperature and °C symbol
  display.showNumberDec(displayValue, false);
  delay(500);
}


#include <Wire.h>
#include <TM1637Display.h>
#include <math.h>
#include "MAX30105.h"
#include "heartRate.h"

// ===== PIN DEFINITIONS =====
#define CLK PB6
#define DIO PB7
#define ANALOG_PIN PA0
#define BUTTON_PIN PC13

// ===== OBJECTS =====
TM1637Display display(CLK, DIO);
MAX30105 particleSensor;

// ===== CONSTANTS FOR NTC THERMISTOR =====
const float VREF = 3.3;
const float R_FIXED = 10000; // 10kΩ
const float BETA = 3950;
const float T0 = 298.15;
const float R0 = 10000;

// ===== GLOBAL VARIABLES =====
bool mode = 0; // 0 = Temperature, 1 = Oximeter
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const int debounceDelay = 250;
long lastBeat = 0;

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(ANALOG_PIN, INPUT);

  display.setBrightness(7);
  display.showNumberDec(0);

  Wire.begin();
  delay(100);

  // Initialize MAX30105
  if (!particleSensor.begin(Wire, I2C_SPEED_STANDARD)) {
    Serial.println("MAX30105 not found! Check wiring.");
  } else {
    particleSensor.setup();
    Serial.println("MAX30105 ready.");
  }
}

// ===== MAIN LOOP =====
void loop() {
  handleButton();

  if (mode == 0) {
    showTemperature();
  } else {
    showOximeter();
  }
}

// ===== BUTTON HANDLER =====
void handleButton() {
  bool reading = digitalRead(BUTTON_PIN);
  if (reading == LOW && lastButtonState == HIGH && (millis() - lastDebounceTime) > debounceDelay) {
    mode = !mode; // toggle
    lastDebounceTime = millis();
    display.showNumberDec(9999); // indicate mode switch
    delay(300);
  }
  lastButtonState = reading;
}

// ===== TEMPERATURE DISPLAY =====
void showTemperature() {
  int adcValue = analogRead(ANALOG_PIN);
  float voltage = (adcValue * VREF) / 4095.0;

  if (voltage <= 0.01 || voltage >= VREF) return;

  float R_therm = R_FIXED * (VREF / voltage - 1.0);
  float temperatureK = 1.0 / ((1.0 / T0) + (1.0 / BETA) * log(R_therm / R0));
  float temperatureC = temperatureK - 273.15;

  // Add calibration offset (+27 fine-tuned for your setup)
  int displayValue = (int)(temperatureC + 25 + 0.5);

  Serial.print("Temperature: ");
  Serial.print(temperatureC);
  Serial.println(" °C");

  display.showNumberDec(displayValue, false);
  delay(500);
}

// ===== OXIMETER DISPLAY =====
void showOximeter() {
  long irValue = particleSensor.getIR();

  if (irValue < 50000) {
    Serial.println("No finger detected");
    display.showNumberDec(0);
    delay(300);
    return;
  }

  if (checkForBeat(irValue)) {
    long delta = millis() - lastBeat;
    lastBeat = millis();
    float bpm = 60 / (delta / 1000.0);

    if (bpm > 30 && bpm < 180) {
      Serial.print("BPM: ");
      Serial.println(bpm);
      display.showNumberDec((int)bpm, false);
    }
  }
  delay(50);
}
