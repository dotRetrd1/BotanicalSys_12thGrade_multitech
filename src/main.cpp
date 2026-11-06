#include <Arduino.h>

#define phPin 32
#define ecPin 33

int pins2Print[] = {phPin,ecPin};

void readAndPrint();

void setup() {
  Serial.begin(115200);
  pinMode(phPin,INPUT);
  pinMode(ecPin,INPUT);
}

void loop() {
  //readAndPrint();
}

void readAndPrint() {
  int len = sizeof(pins2Print);
  float reads[len] = {};
  for(int i=0; i<len; i++) {
    reads[i] = analogRead(pins2Print[i]);
    Serial.print("Pin ");
    Serial.print(pins2Print[i]);
    Serial.print(": ");
    Serial.print(reads[i]);
  }
  Serial.print("\n");
}
