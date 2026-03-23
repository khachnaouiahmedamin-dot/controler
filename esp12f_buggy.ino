/*
 ============================================
  TURBO PANTHER — ESP12-F Controller
  WebSocket + PWM (ESC + Servo)
  
  Librairies nécessaires (Arduino IDE) :
  - ESP8266WiFi (incluse)
  - ESPAsyncWebServer  → Gestionnaire de lib
  - ESPAsyncTCP        → Gestionnaire de lib
  - ArduinoJson        → Gestionnaire de lib
 ============================================
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Servo.h>

// ── WiFi Access Point ──
const char* AP_SSID = "TurboPanther";
const char* AP_PASS = "buggy1234";

// ── Pins ──
#define PIN_ESC    D1   // GPIO5 → ESC (signal PWM)
#define PIN_SERVO  D2   // GPIO4 → Servo direction

// ── Config ──
#define ESC_MIN    1000  // µs (stop/frein)
#define ESC_IDLE   1500  // µs (neutre)
#define ESC_MAX    2000  // µs (plein gaz)
#define SERVO_MIN  700   // µs (gauche max)
#define SERVO_MID  1500  // µs (centre)
#define SERVO_MAX  2300  // µs (droite max)

Servo esc;
Servo steer;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

int throttleVal = 0;  // -100..100
int steerVal    = 0;  // -100..100
bool stopped    = false;

// ── Map throttle → µs ESC ──
int throttleToUs(int t) {
  if (stopped) return ESC_IDLE;
  if (t == 0) return ESC_IDLE;
  if (t > 0) return map(t, 0, 100, ESC_IDLE, ESC_MAX);
  return map(t, -100, 0, ESC_MIN, ESC_IDLE);
}

// ── Map steering → µs Servo ──
int steerToUs(int s) {
  return map(s, -100, 100, SERVO_MIN, SERVO_MAX);
}

// ── WebSocket handler ──
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("Client #%u connecté\n", client->id());
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("Client #%u déconnecté\n", client->id());
    // sécurité: stop moteur si client déconnecté
    throttleVal = 0; steerVal = 0;
    esc.writeMicroseconds(ESC_IDLE);
    steer.writeMicroseconds(SERVO_MID);
  } else if (type == WS_EVT_DATA) {
    String msg = String((char*)data).substring(0, len);
    
    StaticJsonDocument<128> doc;
    DeserializationError err = deserializeJson(doc, msg);
    if (err) return;

    // Réception stop
    if (doc.containsKey("stop")) {
      stopped = true;
      throttleVal = 0; steerVal = 0;
    } else {
      stopped = false;
    }

    // Throttle & steering
    if (doc.containsKey("t")) throttleVal = constrain((int)doc["t"], -100, 100);
    if (doc.containsKey("s")) steerVal    = constrain((int)doc["s"], -100, 100);

    // Mode (NORMAL, SPORT, ECO)
    if (doc.containsKey("mode")) {
      String m = doc["mode"].as<String>();
      Serial.println("Mode: " + m);
      // tu peux ajuster les limites selon le mode ici
    }

    // Appliquer
    esc.writeMicroseconds(throttleToUs(throttleVal));
    steer.writeMicroseconds(steerToUs(steerVal));

    // Réponse ping
    client->text("ok");
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  // Init ESC (position neutre obligatoire au démarrage)
  esc.attach(PIN_ESC, ESC_MIN, ESC_MAX);
  esc.writeMicroseconds(ESC_IDLE);
  delay(2000); // ESC calibration delay

  // Init Servo
  steer.attach(PIN_SERVO, SERVO_MIN, SERVO_MAX);
  steer.writeMicroseconds(SERVO_MID);

  // WiFi Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP()); // normalement 192.168.4.1

  // WebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // Serve static (optionnel si tu veux héberger l'HTML sur l'ESP)
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send(200, "text/plain", "TurboPanther OK");
  });

  server.begin();
  Serial.println("Serveur WebSocket démarré sur ws://192.168.4.1/ws");
}

void loop() {
  ws.cleanupClients();
  // Sécurité: watchdog — si pas de message depuis 500ms, stopper
  // (à implémenter si nécessaire)
}
