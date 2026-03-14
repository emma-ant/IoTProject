#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

#define CE_PIN  21
#define CSN_PIN 5
// ================= RF24 =================
RF24 radio(CE_PIN, CSN_PIN);
const byte address[6] = "00001";

// ================= Pins =================
#define LDR_PIN 27
#define LED_PIN 2
#define NODE_ID 1

// ================= Utility =================
float generateRandomValue(float minVal, float maxVal) {
  return random(minVal * 100, maxVal * 100) / 100.0;
}

// ================= Setup =================
void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  radio.begin();
  radio.openWritingPipe(address);
  radio.openReadingPipe(0, address);
  radio.setPALevel(RF24_PA_MIN);
  radio.stopListening();

  Serial.println("Field Device 2 ready");
}

// ================= Loop =================
void loop() {
  // Generate random sensor values
  float t = generateRandomValue(15.0, 40.0); // Temperature between 15°C and 40°C
  float h = generateRandomValue(30.0, 90.0); // Humidity between 30% and 90%
  float l = generateRandomValue(0.0, 100.0); // Light between 0 and 100%]
  float pred = generateRandomValue(0.0, 1.0); // Light between 0 and 100%

  char payload[32]; // nRF24 max payload

  // Format: node,temp,hum,light
  snprintf(
    payload,
    sizeof(payload),
    "%d,%.2f,%.2f,%.2f,%.2f", 
    NODE_ID, t, h, l,pred
  );

  radio.write(payload, strlen(payload) + 1);
  bool ok = radio.write(payload, strlen(payload) + 1);

  Serial.print("TX Payload: ");
  Serial.println(payload);
  Serial.println(ok ? "RF TX OK" : "RF TX FAIL");

  // Listen for LED control commands
  radio.startListening();
  unsigned long start = millis();
  while (millis() - start < 150) {
    if (radio.available()) {
      char cmd[32] = {0};
      radio.read(cmd, sizeof(cmd));

      int node, led;
      int fields = sscanf(cmd, "%d,%d", &node, &led);

      if (fields == 2 && node == NODE_ID) {
        digitalWrite(LED_PIN, led ? HIGH : LOW);
        Serial.print("LED CMD RX: ");
        Serial.println(cmd);
      }
    }
  }
  radio.stopListening();
  delay(1000);
}
