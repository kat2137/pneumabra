#include <Wire.h>
#include <Adafruit_MPRLS.h>

#define VALVE_PIN 13
#define PUMP_PIN  14
#define RST_PIN   16
#define EOC_PIN  -1

// Correct constructor:
Adafruit_MPRLS mpr = Adafruit_MPRLS(RST_PIN, EOC_PIN);

void setup() {
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(VALVE_PIN, OUTPUT);

  Serial.begin(115200);
  Wire.begin(4, 5);  

  while (!Serial) {}

  Serial.println("Initializing MPRLS...");
  
  if (!mpr.begin()) {
    Serial.println("Failed to find MPRLS sensor!");
    while (1); 
  }

  Serial.println("MPRLS initialized!");
  
}


void loop() {
  float pressure_hPa = mpr.readPressure();

  Serial.print("Pressure: ");
  Serial.print(pressure_hPa);
  Serial.println(" hPa");

  digitalWrite(PUMP_PIN, HIGH);
  digitalWrite(VALVE_PIN, HIGH);
  delay(8000);


  digitalWrite(PUMP_PIN, LOW);
  digitalWrite(VALVE_PIN, LOW);
  delay(1000);
}
