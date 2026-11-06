#include <Arduino.h>
#include <BluetoothSerial.h>
#include <math.h>
#include <stdlib.h>

BluetoothSerial SerialBT;

constexpr int tempPin = 32;
constexpr int phPin = 33;

float expectedPh = 7.0f;
float expectedTemp = 0.0f;
float expectedEc = 0.0f;
float expectedWaterLevel = 0.0f;

float currentPh = 0.0f;

int readAndPrint(int pin, bool printValue);
float calculatePh(int raw);
bool parseFloatValue(String text, float& value);
void updateWantedValue(int command, float value);
void communicate();
void closeLoop(float expected, float actual, float tolerance);

void setup() {
  Serial.begin(115200);
  SerialBT.begin("Ido");
  SerialBT.setTimeout(50);
  analogReadResolution(12);

  pinMode(phPin, INPUT);
  pinMode(tempPin, INPUT);
}

void loop() {
  currentPh = calculatePh(readAndPrint(phPin, false));
  communicate();
  closeLoop(expectedPh, currentPh, 0.1f);
  delay(10);
}

bool parseFloatValue(String text, float& value) {
  text.trim();
  if (text.isEmpty()) {
    return false;
  }

  const char* start = text.c_str();
  char* end = nullptr;
  float parsedValue = strtof(start, &end);
  if (end == start || *end != '\0' || !isfinite(parsedValue)) {
    return false;
  }

  value = parsedValue;
  return true;
}

void communicate() {
  if (!SerialBT.available()) {
    return;
  }

  String commandLine = SerialBT.readStringUntil('\n');
  commandLine.trim();
  if (commandLine.isEmpty()) {
    return;
  }

  int separator = commandLine.indexOf(':');
  if (separator < 0) separator = commandLine.indexOf(',');
  if (separator < 0) separator = commandLine.indexOf(' ');

  String commandText = separator < 0 ? commandLine : commandLine.substring(0, separator);
  int command = commandText.toInt();
  float requestedValue = 0.0f;
  bool hasValue = separator >= 0 && parseFloatValue(commandLine.substring(separator + 1), requestedValue);

  switch (command) {
    case 200:
      SerialBT.println(currentPh, 2);
      break;
    case 201:
    case 202:
    case 203:
      SerialBT.println("NOT_IMPLEMENTED");
      break;
    case 101:
    case 102:
    case 103:
    case 104:
      if (!hasValue || (command == 101 && (requestedValue < 0.0f || requestedValue > 14.0f))) {
        SerialBT.println("Invalid target; use command:value (pH range 0-14)");
        break;
      }
      updateWantedValue(command, requestedValue);
      SerialBT.println("Target updated");
      break;
    default:
      SerialBT.println("Unknown command");
      break;
  }
}

int readAndPrint(int pin, bool printValue) {
  int value = analogRead(pin);
  if (printValue) {
    Serial.print("Pin ");
    Serial.print(pin);
    Serial.print(": ");
    Serial.print(value);
    Serial.print(" | ");
  }
  return value;
}

float calculatePh(int raw) {
  return (float)raw * (14.0 / 4095.0);
}

void updateWantedValue(int command, float value) {
  switch (command) {
    case 101:
      expectedPh = value;
      break;
    case 102:
      expectedTemp = value;
      break;
    case 103:
      expectedEc = value;
      break;
    case 104:
      expectedWaterLevel = value;
      break;
  }
}

void closeLoop(float expected, float actual, float tolerance) {
  static int previousStatus = 2;
  float difference = expected - actual;
  int status = fabsf(difference) <= tolerance ? 0 : (difference > 0.0f ? 1 : -1);

  if (status == previousStatus) {
    return;
  }

  if (status > 0) {
    Serial.println("pH low: increase");
    SerialBT.println("pH low: increase");
  } else if (status < 0) {
    Serial.println("pH high: decrease");
    SerialBT.println("pH high: decrease");
  } else {
    Serial.println("pH stable");
    SerialBT.println("pH stable");
  }

  previousStatus = status;
}

// Queries: 200-203. Set targets with 101-104 followed by ':' and a value, e.g. 101:7.2.