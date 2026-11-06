#include <Arduino.h>
#include <BluetoothSerial.h>

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif
BluetoothSerial SerialBT;


#define tempPin 32
#define phPin 33

int readAndPrint(int pin, bool print);
float calculatePh(int raw);
void communicate(float phValue, int tempValue, int ecValue, int waterLevelValue);

void setup() {
  Serial.begin(115200);
  SerialBT.begin("Ido");
  delay(1000);

  pinMode(phPin, INPUT);
  pinMode(tempPin, INPUT);
}

void loop() {
  int rawPh = readAndPrint(phPin, false);
  float phValue = calculatePh(rawPh);
  
  //Serial.print("Calculated pH: ");
  //Serial.println(phValue);

  communicate(phValue, 0, 0, 0);

}

void communicate(float phValue = 0.0, int tempValue = 0, int ecValue = 0, int waterLevelValue = 0) {
  if (SerialBT.available()) {
    String commandStr = SerialBT.readStringUntil('\n');
    commandStr.trim(); 
    
    int command = -1; // Default to an invalid command

    if (commandStr.length() > 0) {
      command = commandStr.toInt(); 
      Serial.print("Received command: ");
      Serial.println(command);
    }
  
    switch (command) {
      case 200: // Request pH value
        SerialBT.print(phValue);
        break;
      case 201: // Request temperature value
        SerialBT.print(tempValue);
        break;
      case 202: // Request EC value
        // Implement EC reading logic here
        SerialBT.println("EC reading not implemented");
        break;
      case 203: // Request water level value
        // Implement water level reading logic here
        SerialBT.println("Water level reading not implemented");
        break;
      case 101: // Send wanted pH value
        // Implement logic to receive and store wanted pH value
        SerialBT.println("Wanted pH value received");
        break;
      case 102: // Send wanted temperature value
        // Implement logic to receive and store wanted temperature value
        SerialBT.println("Wanted temperature value received");
        break;
      case 103: // Send wanted EC value
        // Implement logic to receive and store wanted EC value
        SerialBT.println("Wanted EC value received");
        break;
      case 104: // Send wanted water level value
        // Implement logic to receive and store wanted water level value
        SerialBT.println("Wanted water level value received");
        break;
      default:
        SerialBT.println("Unknown command");
        break;
    }
  }
  delay(20);
}


int readAndPrint(int pin,bool print) {
  int value = analogRead(pin); 
  if (print) {
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


// 200 - req ph | 201 - req temp | 202 - req ec | 203 - req water level | 101 - send wanted ph | 102 - send wanted temp | 103 - send wanted ec | 104 - send wanted water level