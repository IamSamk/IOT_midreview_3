#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Firebase_ESP_Client.h>

#include <map>
#include <set>
#include <vector>

// Helper headers shipped with Firebase Arduino library
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// -----------------------
// --- Configuration ----
// -----------------------

const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// Firebase configuration
const char *FIREBASE_DATABASE_URL = "https://iotcie3-default-rtdb.asia-southeast1.firebasedatabase.app";
const char *FIREBASE_DATABASE_SECRET = "4ogjIrdOy635PgMK5sgcW1427fDReNvSrxgUg3yJ"; // Legacy DB secret

// CoinLayer configuration
const char *COIN_LAYER_API_KEY = "YOUR_COIN_LAYER_API_KEY"; // Provide via build flags for production
const unsigned long PRICE_CACHE_TTL_MS = 30000;               // 30s cache window
const unsigned long SUBSCRIPTION_REFRESH_MS = 60000;          // Refresh subscriptions every minute

// Hardware pins (adjust to actual wiring)
const uint8_t LED_PIN = 2;
const uint8_t BUZZER_PIN = 12;
const uint8_t BUZZER_CHANNEL = 0;

struct Threshold {
  String uid;
  String email;
  String symbol;
  float lower;
  float upper;
};

struct PriceCacheEntry {
  float price;
  unsigned long fetchedAt;
};

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

std::vector<Threshold> thresholds;
std::set<String> uniqueSymbols;
std::map<String, PriceCacheEntry> priceCache;
std::map<String, bool> alertLatch; // Prevent repeated alerts without reset

unsigned long lastSubscriptionRefresh = 0;

// -----------------------
// --- Helper Methods ---
// -----------------------

void connectWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
}

void initFirebase()
{
  config.database_url = FIREBASE_DATABASE_URL;
  config.signer.tokens.legacy_token = FIREBASE_DATABASE_SECRET;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

float fetchPriceForSymbol(const String &symbol)
{
  const unsigned long now = millis();
  if (priceCache.count(symbol) && (now - priceCache[symbol].fetchedAt) < PRICE_CACHE_TTL_MS)
  {
    return priceCache[symbol].price;
  }

  HTTPClient https;
  WiFiClientSecure client;
  client.setInsecure(); // NOTE: for production, validate the certificate fingerprint

  String url = String("https://api.coinlayer.com/live?access_key=") + COIN_LAYER_API_KEY + "&symbols=" + symbol;
  if (!https.begin(client, url))
  {
    Serial.println("Failed to configure CoinLayer request");
    return NAN;
  }

  const int httpCode = https.GET();
  if (httpCode != HTTP_CODE_OK)
  {
    Serial.printf("CoinLayer error %d for %s\n", httpCode, symbol.c_str());
    https.end();
    return NAN;
  }

  const String payload = https.getString();
  https.end();

  DynamicJsonDocument doc(2048);
  auto error = deserializeJson(doc, payload);
  if (error)
  {
    Serial.println("Failed to parse CoinLayer response");
    return NAN;
  }

  if (!doc["success"].as<bool>())
  {
    Serial.println("CoinLayer replied with failure");
    return NAN;
  }

  float price = doc["rates"][symbol];
  priceCache[symbol] = {price, now};
  return price;
}

void logAlert(const Threshold &threshold, float price, const String &direction)
{
  FirebaseJson alertJson;
  alertJson.set("uid", threshold.uid);
  alertJson.set("email", threshold.email);
  alertJson.set("symbol", threshold.symbol);
  alertJson.set("price", price);
  alertJson.set("direction", direction);
  alertJson.set("triggeredAt", Firebase.getCurrentTime());

  String alertPath = String("/alerts/");
  if (Firebase.RTDB.pushJSON(&fbdo, alertPath.c_str(), alertJson))
  {
    Serial.printf("Alert logged for %s (%s)\n", threshold.email.c_str(), threshold.symbol.c_str());
  }
  else
  {
    Serial.printf("Failed to log alert: %s\n", fbdo.errorReason().c_str());
  }
}

void triggerHardwareAlert(bool status)
{
  digitalWrite(LED_PIN, status ? HIGH : LOW);
  if (status)
  {
    ledcWriteTone(BUZZER_CHANNEL, 2000);
  }
  else
  {
    ledcWriteTone(BUZZER_CHANNEL, 0);
  }
}

void updateCoinCache(const String &symbol, float price)
{
  FirebaseJson payload;
  payload.set("symbol", symbol);
  payload.set("lastPrice", price);
  payload.set("updatedAt", Firebase.getCurrentTime());

  String path = String("/coins/") + symbol;
  if (!Firebase.RTDB.updateNode(&fbdo, path.c_str(), payload))
  {
    Serial.printf("Failed to update cache for %s: %s\n", symbol.c_str(), fbdo.errorReason().c_str());
  }
}

void refreshSubscriptions()
{
  Serial.println("Refreshing subscriptions from Firebase...");
  thresholds.clear();
  uniqueSymbols.clear();

  if (!Firebase.RTDB.getJSON(&fbdo, "/users"))
  {
    Serial.printf("Failed to fetch users: %s\n", fbdo.errorReason().c_str());
    return;
  }

  DynamicJsonDocument doc(16384);
  auto err = deserializeJson(doc, fbdo.payload().c_str());
  if (err)
  {
    Serial.printf("Failed to parse subscriptions JSON: %s\n", err.c_str());
    return;
  }

  JsonObject users = doc.as<JsonObject>();
  for (JsonPair user : users)
  {
    const String uid = user.key().c_str();
    JsonObject userObj = user.value().as<JsonObject>();
    if (userObj.isNull())
    {
      continue;
    }

    const char *email = userObj["email"] | "";
    JsonObject subs = userObj["subscriptions"].as<JsonObject>();
    if (subs.isNull())
    {
      continue;
    }

    for (JsonPair subscription : subs)
    {
      const String symbol = subscription.key().c_str();
      JsonObject thresholdNode = subscription.value().as<JsonObject>();
      float lower = thresholdNode["lower"] | 0.0f;
      float upper = thresholdNode["upper"] | 0.0f;

      thresholds.push_back({uid, email, symbol, lower, upper});
      uniqueSymbols.insert(symbol);
    }
  }

  Serial.printf("Loaded %d thresholds across %d unique symbols\n", thresholds.size(), uniqueSymbols.size());
}

void evaluateThresholds()
{
  for (const auto &symbol : uniqueSymbols)
  {
    float price = fetchPriceForSymbol(symbol);
    if (isnan(price))
    {
      continue;
    }

    updateCoinCache(symbol, price);

    for (auto &threshold : thresholds)
    {
      if (threshold.symbol != symbol)
      {
        continue;
      }

      const String latchKey = threshold.uid + ":" + symbol;
      bool isLatched = alertLatch[latchKey];

      if (price >= threshold.upper)
      {
        if (!isLatched)
        {
          triggerHardwareAlert(true);
          logAlert(threshold, price, "upper");
          alertLatch[latchKey] = true;
        }
      }
      else if (price <= threshold.lower)
      {
        if (!isLatched)
        {
          triggerHardwareAlert(true);
          logAlert(threshold, price, "lower");
          alertLatch[latchKey] = true;
        }
      }
      else
      {
        if (isLatched)
        {
          triggerHardwareAlert(false);
          alertLatch[latchKey] = false;
        }
      }
    }
  }
}

// -----------------------
// --- Arduino Setup ----
// -----------------------

void setup()
{
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  ledcSetup(BUZZER_CHANNEL, 2000, 10);
  ledcAttachPin(BUZZER_PIN, BUZZER_CHANNEL);

  connectWiFi();
  initFirebase();
  refreshSubscriptions();
}

void loop()
{
  unsigned long now = millis();
  if ((now - lastSubscriptionRefresh) > SUBSCRIPTION_REFRESH_MS)
  {
    refreshSubscriptions();
    lastSubscriptionRefresh = now;
  }

  evaluateThresholds();
  delay(2000); // Poll interval
}
