#include <Arduino.h>

#include "bluetooth_commands.h"
#include "sensor_manager.h"

void setup() {
  Serial.begin(115200);
  initializeBluetooth();
  initializeSensors();
}

void loop() {
  communicate();
  updateSensors(millis());
  yield();
}
