#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include <DHT.h>
#include "model.h"
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include "shared_packet.h"

#define CE_PIN  21
#define CSN_PIN 5
// RF24 pin decs
RF24 radio(CE_PIN, CSN_PIN);
const byte address[6] = "00001";

//other sensor pin iniializations
#define DHTPIN 12
#define DHTTYPE DHT22
#define LDR_PIN 27
#define HEATER_PIN 26
#define LED_PIN 2
#define NODE_ID 1

DHT dht(DHTPIN, DHTTYPE);

//TensorFlow Lite interpreter
namespace {
  tflite::ErrorReporter* error_reporter = nullptr;
  tflite::MicroInterpreter* interpreter = nullptr;
  TfLiteTensor* input = nullptr;
  TfLiteTensor* output = nullptr;

  constexpr int kTensorArenaSize = 10000;
  uint8_t tensor_arena[kTensorArenaSize];
}

// Utilizty Scalar
float normalize(float v, float minv, float maxv) {
  return (v - minv) / (maxv - minv);
}

float mapLDRToLight(int v) {
  return constrain(map(v, 0, 4095, 0, 100), 0, 100);
}

// Inference 
float runInference(float t, float h, float l) {
  input->data.f[0] = normalize(t, 15, 40);
  input->data.f[1] = normalize(h, 30, 90);
  input->data.f[2] = normalize(l, 0, 100);

  interpreter->Invoke();
  float pred = output->data.f[0];
  
   bool heaterOn = pred > 0.65; // set average we made for normal chickens

  digitalWrite(HEATER_PIN, heaterOn ? HIGH : LOW);

  Serial.println("------------------------------------------------");
  Serial.printf("Temp: %.2f  Hum: %.2f  Light: %.2f\n",
                t, h, l);
  Serial.printf("Prediction: %.4f -> Heater %s\n",
                pred, heaterOn ? "ON" : "OFF");
  Serial.println("----------------------------------");



  return pred;
}

void setup() {
  Serial.begin(115200);

  pinMode(HEATER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  dht.begin();

  // ---------- RF ----------
  radio.begin();
  radio.openWritingPipe(address);
  radio.openReadingPipe(0, address);
  radio.setPALevel(RF24_PA_MIN);
  radio.stopListening();

  // ---------- TFLite ----------
  static tflite::MicroErrorReporter micro_error_reporter;
  static tflite::AllOpsResolver resolver;
  const tflite::Model* model = tflite::GetModel(model_data);

  static tflite::MicroInterpreter static_interpreter(
    model, resolver, tensor_arena, kTensorArenaSize, &micro_error_reporter
  );

  interpreter = &static_interpreter;
  interpreter->AllocateTensors();
  input = interpreter->input(0);
  output = interpreter->output(0);

  Serial.println("Field node ready");
}

void loop() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  float l = mapLDRToLight(analogRead(LDR_PIN));
  if (isnan(t) || isnan(h)) return;

  float pred = runInference(t, h, l);
  
 


char payload[32];   // nRF24 max payload

uint8_t heater = (pred > 0.65) ? 1 : 0;

// Format: node,temp,hum,light,pred
snprintf(
  payload,
  sizeof(payload),
  "%d,%.1f,%.1f,%.1f,%d",
  NODE_ID,
  t,
  h,
  l,
  heater
);

radio.write(payload, strlen(payload) + 1);
bool ok = radio.write(payload, strlen(payload) + 1);

Serial.print("TX Payload: ");
Serial.println(payload);
Serial.println(ok ? "RF TX OK" : "RF TX FAIL");
  Serial.println(ok ? "RF TX OK" : "RF TX FAIL");

  // ---------- Listen briefly ----------
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
