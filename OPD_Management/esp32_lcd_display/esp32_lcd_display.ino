/*
  esp32_lcd_display.ino
  ========================
  OPD Queue LCD Display for ESP32 + 16x2 I2C LCD

  What it does:
    - Connects to your WiFi
    - Polls your Flask server every 5 seconds
    - Displays current token number and estimated wait time on LCD

  LCD Output:
    Line 1: "NOW: #007      "
    Line 2: "Wait: ~24 min  "

  When no patient is consulting:
    Line 1: "QUEUE IDLE     "
    Line 2: "Wait: ~X min   "

  Required Libraries (install via Arduino Library Manager):
    - LiquidCrystal_I2C  (by Frank de Brabander)
    - ArduinoJson         (by Benoit Blanchon, v6.x)

  Wiring (ESP32 → 16x2 I2C LCD):
    ESP32 3.3V  →  LCD VCC
    ESP32 GND   →  LCD GND
    ESP32 GPIO21 (SDA)  →  LCD SDA
    ESP32 GPIO22 (SCL)  →  LCD SCL

  IMPORTANT: Edit the CONFIG section below before uploading.
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>

// ── CONFIG — EDIT THESE ──────────────────────────────────────────────────────
const char* WIFI_SSID     = "YOUR_WIFI_SSID";       // Your WiFi name
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";   // Your WiFi password

// Your Flask server IP and port (find with ipconfig on Windows)
// Example: "http://192.168.1.10:5000"
const char* SERVER_IP     = "http://192.168.X.X:5000";

// Which doctor's queue to display (matches doctor id in Supabase)
const int   DOCTOR_ID     = 1;

// How often to poll the server (milliseconds)
const int   POLL_INTERVAL = 5000;
// ─────────────────────────────────────────────────────────────────────────────

// I2C LCD — 16 columns, 2 rows, I2C address 0x27 (common default)
// If your LCD doesn't work, try 0x3F instead of 0x27
LiquidCrystal_I2C lcd(0x27, 16, 2);

unsigned long lastPollTime = 0;

// ── SETUP ────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();

  // Welcome screen
  lcd.setCursor(0, 0);
  lcd.print("OPD Queue System");
  lcd.setCursor(0, 1);
  lcd.print("Connecting WiFi.");

  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected!");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP().toString());
    delay(2000);
  } else {
    Serial.println("\nWiFi FAILED");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Failed!");
    lcd.setCursor(0, 1);
    lcd.print("Check config...");
    while (true) { delay(1000); }  // halt
  }
}

// ── LOOP ─────────────────────────────────────────────────────────────────────
void loop() {
  unsigned long now = millis();

  // Poll every POLL_INTERVAL milliseconds
  if (now - lastPollTime >= POLL_INTERVAL) {
    lastPollTime = now;
    fetchAndDisplay();
  }
}

// ── FETCH DATA AND DISPLAY ON LCD ────────────────────────────────────────────
void fetchAndDisplay() {
  if (WiFi.status() != WL_CONNECTED) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Lost...");
    return;
  }

  // Build the API URL
  String url = String(SERVER_IP) + "/api/display/current?doctor_id=" + String(DOCTOR_ID);

  HTTPClient http;
  http.begin(url);
  http.setTimeout(4000);  // 4 second timeout

  int statusCode = http.GET();

  if (statusCode == 200) {
    String payload = http.getString();
    Serial.println("Response: " + payload);

    // Parse JSON
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error && doc["success"] == true) {
      JsonObject data = doc["data"];

      int    currentToken   = data["current_token"].isNull() ? -1 : (int)data["current_token"];
      float  estimatedWait  = data["estimated_wait_min"] | 0.0;
      int    waitingCount   = data["waiting_count"] | 0;
      String status         = data["status"] | "idle";

      // ── Build LCD Lines ───────────────────────────────────────
      String line1, line2;

      if (status == "consulting" && currentToken > 0) {
        // Format: "NOW: #007      "
        line1 = "NOW: #";
        if (currentToken < 10)        line1 += "00" + String(currentToken);
        else if (currentToken < 100)  line1 += "0"  + String(currentToken);
        else                           line1 += String(currentToken);
      } else {
        line1 = "QUEUE IDLE";
      }

      // Format: "Wait: ~24 min  "
      line2 = "Wait: ~" + String((int)estimatedWait) + " min";

      // ── Update LCD ────────────────────────────────────────────
      lcd.clear();

      // Line 1 (pad to 16 chars)
      lcd.setCursor(0, 0);
      lcd.print(padRight(line1, 16));

      // Line 2
      lcd.setCursor(0, 1);
      lcd.print(padRight(line2, 16));

      Serial.println("LCD: [" + line1 + "] [" + line2 + "]");
    } else {
      // JSON parse error
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print("Parse Error");
      Serial.println("JSON Error: " + String(error.c_str()));
    }

  } else {
    // HTTP error
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Server Error");
    lcd.setCursor(0, 1); lcd.print("Code: " + String(statusCode));
    Serial.println("HTTP Error: " + String(statusCode));
  }

  http.end();
}

// ── UTILITY: Pad string to fixed length with spaces ──────────────────────────
String padRight(String s, int length) {
  while (s.length() < (unsigned int)length) s += " ";
  if (s.length() > (unsigned int)length) s = s.substring(0, length);
  return s;
}
