#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <FS.h>
#include <ArduinoJson.h>

// ---------- CONFIG WIFI ----------
const char* WIFI_SSID = "TU_SSID";
const char* WIFI_PASS = "TU_PASSWORD";

// ---------- WEB SERVER ----------
WebServer server(80);

// ---------- SIMULACIÓN DHT22 ----------
static float curTemp = 24.0f;
static float curHum  = 48.0f;

static float clampf(float v, float lo, float hi){ return v < lo ? lo : (v > hi ? hi : v); }
static void stepSimulation(){
  // variación suave
  curTemp = clampf(curTemp + ((int32_t)esp_random()%7 - 3) * 0.05f, 18.0f, 32.0f);
  curHum  = clampf(curHum  + ((int32_t)esp_random()%7 - 3) * 0.10f, 30.0f, 80.0f);
}

// ---------- STATIC FILES ----------
String contentType(const String& path){
  if (path.endsWith(".html")) return "text/html";
  if (path.endsWith(".css"))  return "text/css";
  if (path.endsWith(".js"))   return "application/javascript";
  if (path.endsWith(".json")) return "application/json";
  if (path.endsWith(".svg"))  return "image/svg+xml";
  if (path.endsWith(".png"))  return "image/png";
  if (path.endsWith(".jpg") || path.endsWith(".jpeg")) return "image/jpeg";
  if (path.endsWith(".ico"))  return "image/x-icon";
  return "text/plain";
}

bool handleFileRead(String path){
  if (path.endsWith("/")) path += "index.html";
  if (!SPIFFS.exists(path)) return false;
  File file = SPIFFS.open(path, "r");
  if (!file) return false;
  server.streamFile(file, contentType(path));
  file.close();
  return true;
}

// ---------- ROUTES ----------
void handleApiData(){
  stepSimulation();
  StaticJsonDocument<128> doc;
  doc["temp"] = curTemp;
  doc["hum"]  = curHum;
  doc["ts"]   = (uint64_t) (millis() + (uint64_t) (time(nullptr)) * 1000ULL); // aproximado
  String out; serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleNotFound(){
  String path = server.uri();
  if (handleFileRead(path)) return;
  server.send(404, "text/plain", "404");
}

void setup(){
  Serial.begin(115200);
  delay(100);

  // FS
  if (!SPIFFS.begin(true)){
    Serial.println("Fallo al montar SPIFFS");
  } else {
    Serial.println("SPIFFS OK");
  }

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Conectando");
  while (WiFi.status() != WL_CONNECTED){
    delay(400);
    Serial.print(".");
  }
  Serial.printf("\nWiFi OK. IP: %s\n", WiFi.localIP().toString().c_str());

  // Rutas
  server.on("/api/data", HTTP_GET, handleApiData);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Servidor iniciado");
}

void loop(){
  server.handleClient();
}
