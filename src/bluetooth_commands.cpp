#include "bluetooth_commands.h"

#include <Arduino.h>
#include <BluetoothSerial.h>
#include <stdlib.h>
#include <string.h>

#include "sensor_manager.h"

namespace {

constexpr uint32_t MIN_INTERVAL_SECONDS[] = {60, 1, 10, 1};
constexpr uint32_t MAX_INTERVAL_SECONDS[] = {86400, 86400, 86400, 3600};
constexpr size_t COMMAND_BUFFER_SIZE = 48;

BluetoothSerial serialBluetooth;
char commandBuffer[COMMAND_BUFFER_SIZE];
size_t commandLength = 0;
bool discardCommand = false;

bool parseFloatValue(const char* text, float& value);
bool parseIntervalSeconds(const char* text, uint32_t& seconds);
void handleCommand(char* commandLine);

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

bool parseIntervalSeconds(const char* text, uint32_t& seconds) {
  if (*text == '-') {
    return false;
  }

  char* end = nullptr;
  unsigned long parsedSeconds = strtoul(text, &end, 10);
  if (end == text) {
    return false;
  }
  while (*end == ' ' || *end == '\t') {
    ++end;
  }
  if (*end != '\0' || parsedSeconds == 0 ||
      parsedSeconds > UINT32_MAX / 1000UL) {
    return false;
  }

  seconds = static_cast<uint32_t>(parsedSeconds);
  return true;
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
    serialBluetooth.println("Unknown command");
    return;
  }

  if (command >= 200 && command <= 203) {
    printSensorValue(sensors[command - 200], serialBluetooth);
    return;
  }

  if (command >= 210 && command <= 213) {
    serialBluetooth.print(sensors[command - 210].intervalMs / 1000UL);
    serialBluetooth.println(" seconds");
    return;
  }

  if (command == 500) {
    for (size_t index = 0; index < SENSOR_COUNT; ++index) {
      if (index > 0) {
        serialBluetooth.print(" | ");
      }
      serialBluetooth.print(sensors[index].name);
      serialBluetooth.print(": ");
      printSensorValue(sensors[index], serialBluetooth);
      serialBluetooth.print(" | target: ");
      serialBluetooth.print(sensors[index].target, 2);
      serialBluetooth.print(" | interval: ");
      serialBluetooth.print(sensors[index].intervalMs / 1000UL);
      serialBluetooth.print("s");
    }
    serialBluetooth.println();
    return;
  }

  if (command >= 301 && command <= 304) {
    size_t index = static_cast<size_t>(command - 301);
    uint32_t intervalSeconds = 0;
    if (valueText == nullptr ||
        !parseIntervalSeconds(valueText, intervalSeconds) ||
        intervalSeconds < MIN_INTERVAL_SECONDS[index] ||
        intervalSeconds > MAX_INTERVAL_SECONDS[index]) {
      serialBluetooth.println("Invalid interval in seconds");
      return;
    }

    sensors[index].intervalMs = intervalSeconds * 1000UL;
    sensors[index].lastReadAt = millis();
    sensors[index].hasRead = true;
    serialBluetooth.print("Interval updated to ");
    serialBluetooth.print(intervalSeconds);
    serialBluetooth.println(" seconds");
    return;
  }

  if (command >= 101 && command <= 104) {
    size_t index = static_cast<size_t>(command - 101);
    float requestedValue = 0.0f;
    if (valueText == nullptr ||
        !parseFloatValue(valueText, requestedValue) ||
        requestedValue < sensors[index].minimumTarget ||
        requestedValue > sensors[index].maximumTarget ||
        (index == 3 && requestedValue != 0.0f && requestedValue != 1.0f)) {
      serialBluetooth.println("Invalid target value");
      return;
    }

    sensors[index].target = requestedValue;
    serialBluetooth.println("Target updated");
    evaluateControlStatus(sensors[index]);
    return;
  }

  serialBluetooth.println("Unknown command");
}

}  // namespace

void initializeBluetooth() {
  serialBluetooth.begin("Ido");
}

void communicate() {
  while (serialBluetooth.available()) {
    char character = static_cast<char>(serialBluetooth.read());
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
      serialBluetooth.println("Command too long");
    }
  }
}
