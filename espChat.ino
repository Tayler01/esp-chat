#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <Update.h>
#include "esp_log.h"

// Enable WiFi debug output if needed
#define DEBUG_WIFI 0
// Show stored credentials when using !show
#define SHOW_CREDENTIALS 0

Preferences prefs;

const char* DEFAULT_NAME = "Caleb";
const char* DEFAULT_ID = "esp32-002";
const char* DEFAULT_COLOR = "#10B981";
const char* DEFAULT_SSID = "Camper1407";
const char* DEFAULT_PASS = "Taylerkid@1407";

const char* supabaseUrl = "https://qymcapixzxuvzcgdjksq.supabase.co/rest/v1/messages";
const char* supabaseKey = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InF5bWNhcGl4enh1dnpjZ2Rqa3NxIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NDk3NTU4MTIsImV4cCI6MjA2NTMzMTgxMn0.GNeDHArcgJZNQpKQOL7DDRQJqbwQ5YwcQOOTsL6TvuE";

#define CURRENT_VERSION "1.0.1"
const char* updateQueryUrl = "https://qymcapixzxuvzcgdjksq.supabase.co/rest/v1/firmware_updates?select=version,binary_url&order=version.desc&limit=1";
String userName, userId, avatarColor, ssid, password;
String lastTimestamp = "";
unsigned long lastPoll = 0;
unsigned long pollInterval = 5000;
unsigned long lastUpdateCheck = 0;
unsigned long updateCheckInterval = 3600000;

WiFiClientSecure client;

// === Utility ===
String formatTimestamp(const String& iso) {
  if (iso.length() < 16) return "--:--";
  return iso.substring(11, 16);
}

String urlEncode(const String& str) {
  String encoded = "";
  char c;
  char bufHex[4];
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (isalnum(c)) {
      encoded += c;
    } else {
      sprintf(bufHex, "%%%02X", (unsigned char)c);
      encoded += bufHex;
    }
  }
  return encoded;
}

// === Settings ===
void loadSettings() {
  prefs.begin("chat", true);
  userName = prefs.getString("name", DEFAULT_NAME);
  userId = prefs.getString("id", DEFAULT_ID);
  avatarColor = prefs.getString("color", DEFAULT_COLOR);
  ssid = prefs.getString("ssid", DEFAULT_SSID);
  password = prefs.getString("pass", DEFAULT_PASS);
  prefs.end();
}

void saveSetting(const char* key, const String& val) {
  prefs.begin("chat", false);
  prefs.putString(key, val);
  prefs.end();
}

void resetSettings() {
  prefs.begin("chat", false);
  prefs.clear();
  prefs.end();
  delay(1000);
  ESP.restart();
}

void clearWifiOnVersionChange() {
  prefs.begin("chat", false);
  String fwStored = prefs.getString("fw_version", "");
  if (fwStored != CURRENT_VERSION) {
    prefs.remove("ssid");
    prefs.remove("pass");
    prefs.putString("fw_version", CURRENT_VERSION);
    Serial.println(F("🚀 Detected new firmware. Cleared saved credentials."));
  }
  prefs.end();
}

void handleCommand(String cmd);
void sendMessage(String content);
void fetchMessages();
void clearWifiOnVersionChange();


// === Core Setup ===
void setup() {
  esp_log_level_set("*", ESP_LOG_WARN);
  esp_log_level_set("wifi", ESP_LOG_NONE);

  Serial.begin(115200);
  delay(500);

  Serial.print(F("\xF0\x9F\x93\x86 Firmware version: "));
  Serial.println(F(CURRENT_VERSION));

  clearWifiOnVersionChange();

  loadSettings();
  Serial.println(F("📘 Type !help for editable settings and commands"));

  Serial.print(F("🔌 Connecting to ChatBox: "));
#if DEBUG_WIFI
  Serial.println(ssid);
#endif
  WiFi.begin(ssid.c_str(), password.c_str());

  unsigned long start = millis();
  const unsigned long wifiTimeout = 15000;

  while (WiFi.status() != WL_CONNECTED && millis() - start < wifiTimeout) {
    delay(250);
    Serial.print(F("."));
    if (Serial.available()) {
      String input = Serial.readStringUntil('\n');
      input.trim();
      if (input.startsWith("!")) handleCommand(input);
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("\n⚠️ Failed to connect to saved Network. Retrying with default credentials..."));
    WiFi.begin(DEFAULT_SSID, DEFAULT_PASS);
    start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < wifiTimeout) {
      delay(250);
      Serial.print(F("."));
      if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        if (input.startsWith("!")) handleCommand(input);
      }
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("\n✅ ChatBox Connected!"));
    client.setInsecure();
    Serial.print(F("📡 IP Address: "));
    Serial.println(WiFi.localIP());
    checkForUpdate();
    Serial.println(F("📡 ESP32 Chat Terminal Ready"));
    Serial.println(F("⌨️ Type to send. Incoming messages will appear below."));
  } else {
    Serial.println(F("\n❌ No Network connection."));
    Serial.println(F("⚠️ Offline mode: Commands available, messaging disabled."));
    Serial.println(F("🛠️ Use !setssid / !setpass and !reboot to fix."));
  }
}

// === Main Loop ===
void loop() {
  static unsigned long lastCheck = 0;
  static bool triedDefault = false;
  static bool reconnecting = false;
  static unsigned long reconnectStart = 0;

  // 🔹 Handle serial input
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.startsWith("!")) {
      handleCommand(input);
    } else if (input.length() > 0 && WiFi.status() == WL_CONNECTED) {
      sendMessage(input);
    } else if (WiFi.status() != WL_CONNECTED) {
      Serial.println(F("⚠️ Not connected. Message ignored."));
    }
  }

  // 🔹 Handle WiFi reconnect without blocking
  if (WiFi.status() != WL_CONNECTED) {
    if (!reconnecting && millis() - lastCheck > 10000) {
      lastCheck = millis();
      Serial.println(F("🔄 Network disconnected. Retrying..."));

      if (!triedDefault) {
        Serial.print(F("🔌 Retrying saved Network: "));
        Serial.println(ssid);
        WiFi.begin(ssid.c_str(), password.c_str());
      } else {
        Serial.print(F("🔌 Retrying default Network: "));
        Serial.println(DEFAULT_SSID);
        WiFi.begin(DEFAULT_SSID, DEFAULT_PASS);
      }

      reconnecting = true;
      reconnectStart = millis();
    } else if (reconnecting && millis() - reconnectStart > 5000) {
      if (WiFi.status() != WL_CONNECTED && !triedDefault) {
        Serial.println(F("⚠️ Failed. Falling back to default Network..."));
        WiFi.begin(DEFAULT_SSID, DEFAULT_PASS);
        triedDefault = true;
        reconnectStart = millis();
      } else {
        reconnecting = false;
      }
    }
  } else {
    reconnecting = false;
  }

  // 🔁 Poll messages via REST every 5s
  if (WiFi.status() == WL_CONNECTED) {
    if (triedDefault) triedDefault = false;

    if (millis() - lastPoll > pollInterval) {
      lastPoll = millis();
      fetchMessages();
    }
    if (millis() - lastUpdateCheck > updateCheckInterval) {
      lastUpdateCheck = millis();
      checkForUpdate();
    }
  }
}


// === Commands ===
void handleCommand(String cmd) {
  if (cmd.startsWith("!setname ")) {
    userName = cmd.substring(9);
    saveSetting("name", userName);
    Serial.println(F("✅ Name updated"));
  } else if (cmd.startsWith("!setid ")) {
    userId = cmd.substring(7);
    saveSetting("id", userId);
    Serial.println(F("✅ ID updated"));
  } else if (cmd.startsWith("!setcolor ")) {
    avatarColor = cmd.substring(10);
    saveSetting("color", avatarColor);
    Serial.println(F("✅ Color updated"));
  } else if (cmd.startsWith("!setssid ")) {
    ssid = cmd.substring(9);
    saveSetting("ssid", ssid);
    Serial.println(F("✅ SSID saved. Use !reboot to apply."));
  } else if (cmd.startsWith("!setpass ")) {
    password = cmd.substring(9);
    saveSetting("pass", password);
    Serial.println(F("✅ Password saved. Use !reboot to apply."));
  } else if (cmd == "!reboot") {
    Serial.println(F("♻️ Rebooting..."));
    delay(1000);
    ESP.restart();
  } else if (cmd == "!reset") {
    Serial.println(F("🧹 Resetting to defaults..."));
    resetSettings();
  } else if (cmd == "!ip") {
    Serial.print(F("📡 IP Address: "));
    Serial.println(WiFi.localIP());
  } else if (cmd == "!show") {
    Serial.println(F("📦 Current Settings:"));
    Serial.println(" userName: " + userName);
    Serial.println(" userId: " + userId);
    Serial.println(" avatarColor: " + avatarColor);
    Serial.println(" ssid: " + String(SHOW_CREDENTIALS ? ssid : "[hidden]"));
    Serial.println(" password: " + String(SHOW_CREDENTIALS ? password : "[hidden]"));
  } else if (cmd == "!help") {
    Serial.println(F("📘 Available commands:"));
    Serial.println(F(" !setname [name] – set display name"));
    Serial.println(F(" !setid [id] – set user ID"));
    Serial.println(F(" !setcolor [hex] – set avatar color"));
    Serial.println(F(" !setssid [ssid] – set SSID (requires !reboot)"));
    Serial.println(F(" !setpass [pass] – set password (requires !reboot)"));
    Serial.println(F(" !show – show current config values"));
    Serial.println(F(" !reset – reset all settings to default"));
    Serial.println(F(" !reboot – restart device"));
    Serial.println(F(" !ip – show IP address"));
  } else {
    Serial.println(F("❓ Unknown command"));
  }
}



// === Messaging ===
void sendMessage(String content) {

  HTTPClient https;
  https.begin(client, supabaseUrl);
  https.addHeader("Content-Type", "application/json");
  https.addHeader("apikey", supabaseKey);
  https.addHeader("Authorization", "Bearer " + String(supabaseKey));

  size_t cap = JSON_OBJECT_SIZE(4) +
                content.length() + userName.length() +
                userId.length() + avatarColor.length() + 64;
  DynamicJsonDocument doc(cap);
  doc["content"] = content;
  doc["user_name"] = userName;
  doc["user_id"] = userId;
  doc["avatar_color"] = avatarColor;

  String json;
  serializeJson(doc, json);

  // POST the message and expect an HTTP 201 Created response
  int code = https.POST(json);
  String resp = https.getString();
  if (code == 201 || code == 200) {
    Serial.println(F("📤 Sent"));
  } else {
    Serial.printf("❌ Send failed (%d): %s\n", code, resp.c_str());
  }
  https.end();
}

void fetchMessages() {
  HTTPClient https;
  String url = String(supabaseUrl) + "?select=content,user_name,created_at,user_id"
               + "&order=created_at.asc&limit=50";

  if (lastTimestamp.length() > 0 && lastTimestamp.indexOf("T") > 0) {
    url += "&created_at=gt." + urlEncode(lastTimestamp);
  }

  https.begin(client, url);
  https.addHeader("apikey", supabaseKey);
  https.addHeader("Authorization", "Bearer " + String(supabaseKey));

  int httpCode = https.GET();
  String raw = https.getString();

  if (httpCode == 200) {
    const size_t AVG_MSG = 200;
    size_t capacity = JSON_ARRAY_SIZE(50) + 50 * JSON_OBJECT_SIZE(4) + 50 * AVG_MSG;
    DynamicJsonDocument doc(capacity);
    DeserializationError err = deserializeJson(doc, raw);
    if (err) {
      Serial.printf("❌ Fetch JSON parse error: %s\n", err.c_str());
      https.end();
      return;
    }

    for (JsonObject msg : doc.as<JsonArray>()) {
      String ts = msg["created_at"] | "";
      if (ts.length() == 0 || ts <= lastTimestamp) continue;

      String senderId = msg["user_id"] | "";
      String name = msg["user_name"] | "";
      String content = msg["content"] | "";
      String timeStr = formatTimestamp(ts);

      if (senderId == userId) {
        Serial.printf("[%s] ✍️  You: %s\n", timeStr.c_str(), content.c_str());
      } else {
        Serial.printf("[%s] 🟢 %s: %s\n", timeStr.c_str(), name.c_str(), content.c_str());
      }

      lastTimestamp = ts;
    }
  } else {
    Serial.printf("❌ Fetch failed (%d): %s\n", httpCode, raw.c_str());
  }

  https.end();
}


void checkForUpdate() {
  Serial.println(F("🔍 Checking for firmware update..."));
  HTTPClient https;
  https.begin(client, updateQueryUrl);
  https.addHeader("apikey", supabaseKey);
  https.addHeader("Authorization", "Bearer " + String(supabaseKey));
  int code = https.GET();
  String raw = https.getString();

  if (code == 200) {
    DynamicJsonDocument doc(JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(2) + 128);
    DeserializationError err = deserializeJson(doc, raw);
    if (!err && doc.size() > 0) {
      String latest = doc[0]["version"] | "";
      String binUrl = doc[0]["binary_url"] | "";
      if (latest.length() > 0 && binUrl.length() > 0 && latest != CURRENT_VERSION) {
        Serial.printf("⬆️  Updating to version %s (from %s)\n", latest.c_str(), CURRENT_VERSION);
        https.end();

        HTTPClient bin;
        bin.begin(client, binUrl);
        int bCode = bin.GET();
        if (bCode == 200) {
          int total = bin.getSize();
          WiFiClient* stream = bin.getStreamPtr();
          if (Update.begin(total)) {
            size_t written = Update.writeStream(*stream);
            if (written == total) {
              Serial.println(F("✅ Update written"));
            } else {
              Serial.println(F("⚠️ Incomplete write"));
            }
            if (Update.end()) {
              Serial.println(F("🔁 Rebooting to apply update..."));
              ESP.restart();
            } else {
              Update.printError(Serial);
            }
          } else {
            Update.printError(Serial);
          }
        } else {
          Serial.printf("❌ Firmware download failed (%d)\n", bCode);
        }
        bin.end();
        return;
      } else {
        Serial.println(F("ℹ️ Firmware up to date."));
      }
    } else {
      Serial.printf("❌ Update JSON parse error: %s\n", err.c_str());
    }
  } else {
    Serial.printf("❌ Update check failed (%d): %s\n", code, raw.c_str());
  }
  https.end();
}
