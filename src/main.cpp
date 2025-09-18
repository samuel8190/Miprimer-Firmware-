#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <DHTesp.h>

// ======================= CONFIGURACIÓN =======================
const char* WIFI_SSID = "Tobar 2";      
const char* WIFI_PASS = "Homb0549";     
const int DHT_PIN = 4;                  
const unsigned long READ_INTERVAL = 2000; // cada 2 segundos

// ======================= OBJETOS GLOBALES ====================
WebServer server(80);
DHTesp dht;

float lastTemp = NAN;
float lastHum = NAN;
unsigned long lastReadMs = 0;

// ======================= SPIFFS ==============================
String getContentType(String filename) {
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".json")) return "application/json";
  return "text/plain";
}

bool handleFileRead(String path) {
  if (path.endsWith("/")) path += "index.html";
  if (SPIFFS.exists(path)) {
    File file = SPIFFS.open(path, "r");
    server.streamFile(file, getContentType(path));
    file.close();
    return true;
  }
  return false;
}

// ======================= HANDLERS ============================
void handleRoot() {
  if (!handleFileRead("/index.html")) {
    server.send(404, "text/plain", "404: Not Found");
  }
}

void handleApiData() {
  unsigned long now = millis();
  if (now - lastReadMs >= READ_INTERVAL) {
    lastReadMs = now;
    TempAndHumidity data = dht.getTempAndHumidity();
    if (!isnan(data.temperature) && !isnan(data.humidity)) {
      lastTemp = data.temperature;
      lastHum = data.humidity;
      Serial.printf("Temp: %.1f°C, Hum: %.1f%%\n", lastTemp, lastHum);
    } else {
      Serial.println("Error leyendo el DHT22");
    }
  }

  DynamicJsonDocument doc(256);
  doc["temp"] = lastTemp;
  doc["hum"] = lastHum;
  doc["ts"] = millis();

  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
}

void handleNotFound() {
  if (!handleFileRead(server.uri())) {
    server.send(404, "text/plain", "404: Not Found");
  }
}

// ======================= SETUP ===============================
void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println("\nIniciando ESP32 DHT22...");

  if (!SPIFFS.begin(true)) {
    Serial.println("Error montando SPIFFS");
    return;
  }
  Serial.println("SPIFFS montado correctamente");

  dht.setup(DHT_PIN, DHTesp::DHT22);

  Serial.printf("Conectando a %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");
  Serial.print("IP asignada: ");
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/data", HTTP_GET, handleApiData);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("Servidor HTTP iniciado");
}

// ======================= LOOP PRINCIPAL ======================
void loop() {
  server.handleClient();
}
