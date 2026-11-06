#include <Arduino.h>
#include <BluetoothSerial.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

BluetoothSerial SerialBT;

constexpr int tempPin = 32;
constexpr int phPin = 33;
constexpr int ecPin = 34;
constexpr int waterLevelPin = 35;

constexpr uint32_t PH_INTERVAL_MS = 15UL * 60UL * 1000UL;
constexpr uint32_t TEMPERATURE_INTERVAL_MS = 30UL * 1000UL;
constexpr uint32_t EC_INTERVAL_MS = 5UL * 60UL * 1000UL;
constexpr uint32_t WATER_LEVEL_INTERVAL_MS = 1000UL;
constexpr size_t COMMAND_BUFFER_SIZE = 48;

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

  // constructor to initialize default values
  SensorState(const char* sensorName, uint32_t interval, ProbeReader reader,
              float initialTarget, float minimum, float maximum,
              float allowedError, float printThreshold)
      : name(sensorName), intervalMs(interval), readProbe(reader),
        target(initialTarget), minimumTarget(minimum), maximumTarget(maximum),
        tolerance(allowedError), reportThreshold(printThreshold), value(NAN),
        lastPrintedValue(NAN), lastReadAt(0), previousStatus(2), hasRead(false),
        available(false), hasPrintedValue(false), unavailableReported(false) {}
};

float readPhProbe();
float readTemperatureProbe();
float readEcProbe();
float readWaterLevelProbe();
bool parseFloatValue(const char* text, float& value);
void handleCommand(char* commandLine);
void updateSensor(SensorState& sensor, uint32_t now);
void evaluateControlStatus(SensorState& sensor);
void printSensorValue(const SensorState& sensor, Print& output);
void communicate();

SensorState sensors[] = { // Name, Interval, Reader, Target, Min, Max, Tolerance, ReportThreshold
  {"pH", PH_INTERVAL_MS, readPhProbe, 7.6f, 0.0f, 14.0f, 0.5f, 0.01f},
  {"Temperature", TEMPERATURE_INTERVAL_MS, readTemperatureProbe, 25.0f, 0.0f, 100.0f, 5.0f, 0.1f},
  {"EC", EC_INTERVAL_MS, readEcProbe, 75.0f, 0.0f, 1000.0f, 0.5f, 1.0f},
  {"Water level", WATER_LEVEL_INTERVAL_MS, readWaterLevelProbe, 0.0f, 0.0f, 1.0f, 0.5f, 0.5f}
};

char commandBuffer[COMMAND_BUFFER_SIZE];
size_t commandLength = 0;
bool discardCommand = false;

void setup() {
  Serial.begin(115200);
  SerialBT.begin("Ido");
  analogReadResolution(12);

  pinMode(phPin, INPUT);
  pinMode(tempPin, INPUT);
  pinMode(ecPin, INPUT);
  pinMode(waterLevelPin, INPUT);

  sensors[0].value = readPhProbe();
  sensors[1].value = readTemperatureProbe();
  sensors[2].value = readEcProbe();
  sensors[3].value = readWaterLevelProbe();
}

void loop() {
  communicate();
  uint32_t now = millis();
  for (SensorState& sensor : sensors) {
    updateSensor(sensor, now);
  }
  yield();
}

bool parseFloatValue(const char* text, float& value) {
  char* end = nullptr;
  float parsedValue = strtof(text, &end);
  if (end == text) {
    return false;
  }

  while (*end == ' ' || *end == '\t') {
    ++end;
  }
  if (*end != '\0' || !isfinite(parsedValue)) {
    return false;
  }

  value = parsedValue;
  return true;
}

void communicate() {
  while (SerialBT.available()) {
    char character = static_cast<char>(SerialBT.read());
    if (character == '\r') {
      continue;
    }

    if (character == '\n') {
      if (!discardCommand && commandLength > 0) {
        commandBuffer[commandLength] = '\0';
        handleCommand(commandBuffer);
      }
      commandLength = 0;
      discardCommand = false;
      continue;
    }

    if (discardCommand) {
      continue;
    }
    if (commandLength < sizeof(commandBuffer) - 1) {
      commandBuffer[commandLength++] = character;
    } else {
      commandLength = 0;
      discardCommand = true;
      SerialBT.println("Command too long");
    }
  }
}

void handleCommand(char* commandLine) {
  while (*commandLine == ' ' || *commandLine == '\t') {
    ++commandLine;
  }
  char* separator = strpbrk(commandLine, ":, \t ");
  char* valueText = nullptr;
  if (separator != nullptr) {
    *separator = '\0';
    valueText = separator + 1;
  }

  char* commandEnd = nullptr;
  long command = strtol(commandLine, &commandEnd, 10);
  if (commandEnd == commandLine || *commandEnd != '\0') {
    SerialBT.println("Unknown command");
    return;
  }

  if (command >= 200 && command <= 203) {
    printSensorValue(sensors[command - 200], SerialBT);
    return;
  }

  if (command == 500) {
    for (size_t index = 0; index < sizeof(sensors) / sizeof(sensors[0]); ++index) {
      if (index > 0) SerialBT.print(" | ");
      SerialBT.print(sensors[index].name);
      SerialBT.print(": ");
      printSensorValue(sensors[index], SerialBT);
      SerialBT.print(" | target: ");
      SerialBT.print(sensors[index].target, 2);
    }
    SerialBT.println();
    return;
  }

  if (command >= 101 && command <= 104) {
    size_t index = static_cast<size_t>(command - 101);
    float requestedValue = 0.0f;
    if (valueText == nullptr || !parseFloatValue(valueText, requestedValue) ||
        requestedValue < sensors[index].minimumTarget ||
        requestedValue > sensors[index].maximumTarget ||
        (index == 3 && requestedValue != 0.0f && requestedValue != 1.0f)) {
      SerialBT.println("Invalid target value");
      return;
    }

    sensors[index].target = requestedValue;
    SerialBT.println("Target updated");
    evaluateControlStatus(sensors[index]);
    return;
  }

  SerialBT.println("Unknown command");
}

void updateSensor(SensorState& sensor, uint32_t now) {
  if (sensor.hasRead && static_cast<uint32_t>(now - sensor.lastReadAt) < sensor.intervalMs) {
    return;
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
    return;
  }

  sensor.value = newValue;
  sensor.available = true;
  sensor.unavailableReported = false;

  if (!sensor.hasPrintedValue || fabsf(newValue - sensor.lastPrintedValue) >= sensor.reportThreshold) {
    Serial.print(sensor.name);
    Serial.print(": ");
    Serial.println(newValue, 2);
    sensor.lastPrintedValue = newValue;
    sensor.hasPrintedValue = true;
  }

  evaluateControlStatus(sensor);
}

void evaluateControlStatus(SensorState& sensor) {
  if (!sensor.available) {
    return;
  }

  float difference = sensor.target - sensor.value;
  int status = fabsf(difference) <= sensor.tolerance ? 0 : (difference > 0.0f ? 1 : -1);
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

float readPhProbe() {
  float rawValue = analogRead(phPin);
  float pHValue = map(rawValue, 0, 4095, 0, 14);
  return pHValue;
}

float readTemperatureProbe() {
  float rawTemperature = analogRead(tempPin);
  float voltage = (rawTemperature / 4095.0) * 3.3; // raw t0o voltage
  float temperatureCelsius = (voltage - 0.5) * 100.0;
  return temperatureCelsius;
}

float readEcProbe() {
  float rawEcValue = analogRead(ecPin);
  // TODO: Convert
  return NAN;
}

float readWaterLevelProbe() {
  bool waterLevelReached = digitalRead(waterLevelPin) == HIGH;
  //1.0 when the level switch is reached, otherwise 0.0.
  return waterLevelReached ? 1.0f : 0.0f;
}

// Queries: 200-203, 500 (all). Set targets with 101-104:value, e.g. 101:7.2.