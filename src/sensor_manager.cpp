#include "sensor_manager.h"
#include <math.h>

namespace {

constexpr int TEMP_PIN = 32;
constexpr int PH_PIN = 33;
constexpr int EC_PIN = 34;
constexpr int WATER_LEVEL_PIN = 35;

constexpr uint32_t PH_INTERVAL_MS = 15UL * 60UL * 1000UL;
constexpr uint32_t TEMPERATURE_INTERVAL_MS = 30UL * 1000UL;
constexpr uint32_t EC_INTERVAL_MS = 5UL * 60UL * 1000UL;
constexpr uint32_t WATER_LEVEL_INTERVAL_MS = 1000UL;

float readPhProbe();
float readTemperatureProbe();
float readEcProbe();
float readWaterLevelProbe();

}  // namespace

SensorState sensors[SENSOR_COUNT] = {
    {"pH", PH_INTERVAL_MS, readPhProbe, 7.6f, 0.0f, 14.0f, 0.5f, 0.01f,
     NAN, NAN, 0, 2, false, false, false, false},
    {"Temperature", TEMPERATURE_INTERVAL_MS, readTemperatureProbe, 25.0f,
     0.0f, 100.0f, 5.0f, 0.1f, NAN, NAN, 0, 2, false, false, false, false},
    {"EC", EC_INTERVAL_MS, readEcProbe, 75.0f, 0.0f, 1000.0f, 0.5f, 1.0f,
     NAN, NAN, 0, 2, false, false, false, false},
    {"Water level", WATER_LEVEL_INTERVAL_MS, readWaterLevelProbe, 0.0f, 0.0f,
     1.0f, 0.5f, 0.5f, NAN, NAN, 0, 2, false, false, false, false},
};

void initializeSensors() {
  analogReadResolution(12);

  pinMode(PH_PIN, INPUT);
  pinMode(TEMP_PIN, INPUT);
  pinMode(EC_PIN, INPUT);
  pinMode(WATER_LEVEL_PIN, INPUT);

  sensors[0].value = readPhProbe();
  sensors[1].value = readTemperatureProbe();
  sensors[2].value = readEcProbe();
  sensors[3].value = readWaterLevelProbe();
}

void updateSensors(uint32_t now) {
  for (SensorState& sensor : sensors) {
    if (sensor.hasRead &&
        static_cast<uint32_t>(now - sensor.lastReadAt) < sensor.intervalMs) {
      continue;
    }

    sensor.hasRead = true;
    sensor.lastReadAt = now;
    float newValue = sensor.readProbe();

    if (!isfinite(newValue)) {
      sensor.value = NAN;
      sensor.available = false;
      sensor.previousStatus = 2;
      if (!sensor.unavailableReported) {
        Serial.print(sensor.name);
        Serial.println(" reading unavailable; configure its probe reader");
        sensor.unavailableReported = true;
      }
      continue;
    }

    sensor.value = newValue;
    sensor.available = true;
    sensor.unavailableReported = false;

    if (!sensor.hasPrintedValue ||
        fabsf(newValue - sensor.lastPrintedValue) >= sensor.reportThreshold) {
      Serial.print(sensor.name);
      Serial.print(": ");
      Serial.println(newValue, 2);
      sensor.lastPrintedValue = newValue;
      sensor.hasPrintedValue = true;
    }

    evaluateControlStatus(sensor);
  }
}

void evaluateControlStatus(SensorState& sensor) {
  if (!sensor.available) {
    return;
  }

  float difference = sensor.target - sensor.value;
  int status = fabsf(difference) <= sensor.tolerance
                   ? 0
                   : (difference > 0.0f ? 1 : -1);
  if (status == sensor.previousStatus) {
    return;
  }

  Serial.print(sensor.name);
  if (status > 0) {
    Serial.println(" is below target");
  } else if (status < 0) {
    Serial.println(" is above target");
  } else {
    Serial.println(" is within target tolerance");
  }
  sensor.previousStatus = status;
}

void printSensorValue(const SensorState& sensor, Print& output) {
  if (!sensor.available) {
    output.print("NOT_READY");
    return;
  }
  output.print(sensor.value, 2);
}

namespace {

float readPhProbe() {
  float rawValue = analogRead(PH_PIN);
  Serial.print("Raw pH value: ");
  Serial.println(rawValue);
  float pHValue = map(rawValue, 0, 4095, 0, 14);
  Serial.print("Mapped pH value: ");
  Serial.println(pHValue);
  return pHValue;
}

float readTemperatureProbe() {
  float rawTemperature = analogRead(TEMP_PIN);
  float voltage = (rawTemperature / 4095.0) * 3.3;
  return (voltage - 0.5) * 100.0;
}

float readEcProbe() {
  float rawEcValue = analogRead(EC_PIN);
  (void)rawEcValue;
  return NAN;
}

float readWaterLevelProbe() {
  bool waterLevelReached = digitalRead(WATER_LEVEL_PIN) == HIGH;
  return waterLevelReached ? 1.0f : 0.0f;
}

}  // namespace
