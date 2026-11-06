#include <Arduino.h>
#include <WiFi.h>

static const char* ssid = "botanicalEsp";
static const char* password = "1234";

WiFiServer wifiServer(5000);

#define tempPin 32
#define phPin 33

int readAndPrint(int pin, bool print);
float calculatePh(int raw);
void communicate();

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid,password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected");
  Serial.print("Ip: ");
  Serial.print(WiFi.localIP());

  wifiServer.begin();

  pinMode(phPin, INPUT);
  pinMode(tempPin, INPUT);
}

void loop() {
  int rawPh = readAndPrint(phPin, false);
  float phValue = calculatePh(rawPh);
  
  Serial.print("Calculated pH: ");
  Serial.println(phValue);

  communicate();

}

void communicate() {
  WiFiClient client = wifiServer.available();
  if (client) {
    // wifi to serial
    while (client.available() > 0)
    {
      char c = Serial.read();
      client.write(c);
    }
  }
  client.stop();
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