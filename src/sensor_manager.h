#pragma once

#include <Arduino.h>

using ProbeReader = float (*)();

struct SensorState {
  const char* name;
  uint32_t intervalMs;
  ProbeReader readProbe;
  float target;
  float minimumTarget;
  float maximumTarget;
  float tolerance;
  float reportThreshold;
  float value;
  float lastPrintedValue;
  uint32_t lastReadAt;
  int previousStatus;
  bool hasRead;
  bool available;
  bool hasPrintedValue;
  bool unavailableReported;
};

constexpr size_t SENSOR_COUNT = 4;
extern SensorState sensors[SENSOR_COUNT];

void initializeSensors();
void updateSensors(uint32_t now);
void evaluateControlStatus(SensorState& sensor);
void printSensorValue(const SensorState& sensor, Print& output);
