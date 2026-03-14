#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include "shared_packet.h"

#define CE_PIN  21
#define CSN_PIN 5
// ================= RF24 =================
RF24 radio(CE_PIN, CSN_PIN);
const byte address[6] = "00001";

// ================= WiFi / MQTT =================
const char* ssid = "DKO222";
const char* password = "Quansah65";
const char* mqtt_server = "172.29.162.61";

WiFiClient espClient;
PubSubClient client(espClient);
WebServer server(80);

SensorPacket lastPacket;
bool hasData = false;

// ================= MQTT =================
void setupMQTT() {
  client.setServer(mqtt_server, 1883);

  Serial.print("MQTT broker: ");
  Serial.println(mqtt_server);

  while (!client.connected()) {
    Serial.print("Connecting MQTT... ");
    if (client.connect("GatewayESP32")) {
      Serial.println("OK");
    } else {
      Serial.print("FAILED, rc=");
      Serial.println(client.state());
      delay(2000);
    }
  }
}

// ================= Web =================
void handleRoot() {
  String html = "<h1>Gateway Dashboard</h1>";
  if (hasData) {
    html += "Temp: " + String(lastPacket.temperature) + "<br>";
    html += "Hum: " + String(lastPacket.humidity) + "<br>";
    html += "Light: " + String(lastPacket.light) + "<br>";
    html += "Prediction: " + String(lastPacket.prediction) + "<br>";
  }
  html += "<a href='/led/on'>LED ON</a><br>";
  html += "<a href='/led/off'>LED OFF</a>";
  server.send(200, "text/html", html);
}

void sendLed(bool state) {
  char cmd[32];
  snprintf(cmd, sizeof(cmd), "%d,%d", 1, state ? 1 : 0);

  radio.stopListening();
  bool ok = radio.write(cmd, strlen(cmd) + 1);
  radio.startListening();

  Serial.print("LED CMD TX: ");
  Serial.println(cmd);
  Serial.println(ok ? "CMD TX OK" : "CMD TX FAIL");

  server.send(200, "text/plain", state ? "LED ON" : "LED OFF");
}


// ================= Setup =================
void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  Serial.println("WiFi connected");

  setupMQTT();

  radio.begin();
  radio.openReadingPipe(0, address);
  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_MIN);
  radio.startListening();

  server.on("/", handleRoot);
  server.on("/led/on", [](){ sendLed(true); });
  server.on("/led/off", [](){ sendLed(false); });
  server.begin();

  Serial.println("Gateway ready");
}

// ================= Loop =================
void loop() {
  server.handleClient();
  client.loop();
if (radio.available()) {
    char rxBuffer[32] = {0};   // IMPORTANT
    radio.read(rxBuffer, sizeof(rxBuffer));

    int node;
    float temp, hum, light;
    int pred;

    int fields = sscanf(
      rxBuffer,
      "%d,%f,%f,%f,%d",
      &node, &temp, &hum, &light, &pred
    );

    if (fields == 5) {
      Serial.print("RF RX: ");
      Serial.println(rxBuffer);

      char payload[128];
      snprintf(payload, sizeof(payload),
        "{\"node\":%d,\"temp\":%.2f,\"hum\":%.2f,\"light\":%.2f,\"pred\":%d}",
        node, temp, hum, light, pred
      );

      client.publish("farm/data", payload);
    } else {
      Serial.print("Bad RF packet: ");
      Serial.println(rxBuffer);
    }
  }
}
