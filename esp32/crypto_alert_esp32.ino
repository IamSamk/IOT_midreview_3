#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Network and Firebase credentials
#define WIFI_SSID "Not connected"
#define WIFI_PASSWORD "12345678"

#define API_KEY "AIzaSyAhQH-phbFZPui0itYuBdTBJIm1FpUsGRQ"
#define DATABASE_URL "https://iotcie3-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define USER_EMAIL "samarthkadambtech24@rvu.edu.in"
#define USER_PASSWORD "SamkRVU@24"

// CoinLayer API
#define COINLAYER_API_KEY "2c54959595d7af478d4c6527fe232d72"

// Hardware pins
#define RED_LED_PIN    12
#define GREEN_LED_PIN  14
#define BLUE_LED_PIN   27
#define BUZZER_PIN     15

// Firebase components
UserAuth user_auth(API_KEY, USER_EMAIL, USER_PASSWORD);
FirebaseApp app;
WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);
RealtimeDatabase Database;

// Timer variables
unsigned long lastFetchTime = 0;
unsigned long lastDisplayTime = 0;
const unsigned long fetchInterval = 60000;
const unsigned long displayInterval = 5000;

// User ID
String userId = "";
String userEmail = USER_EMAIL;
bool userIdFetched = false;

// Store subscription data
struct Subscription {
  String symbol;
  float lower;
  float upper;
  float currentPrice;
  bool hasData;
};

Subscription subscriptions[20];
int subscriptionCount = 0;

// Alert tracking - track types separately
int upperAlerts = 0;  // Count of price increases
int lowerAlerts = 0;  // Count of price decreases

void processData(AsyncResult &aResult);
void fetchCoinPrices();
void fetchUserSubscriptionsFromFirebase();
void checkAlertsAndTrigger(const String &symbol, float price, unsigned long epochSeconds);
void logAlert(const String &symbol, const String &type, float price, unsigned long epochSeconds);
void triggerAlerts();
void displayStatus();
void getUserIdFromAuth();

void setup() {
  Serial.begin(115200);
  
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(BLUE_LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\nWiFi connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  
  ssl_client.setInsecure();
  ssl_client.setConnectionTimeout(1000);
  ssl_client.setHandshakeTimeout(5);
  
  initializeApp(aClient, app, getAuth(user_auth), processData, "authTask");
  app.getApp<RealtimeDatabase>(Database);
  Database.url(DATABASE_URL);
  
  Serial.println("Firebase initialized!");
  Serial.println("Authenticating user...");
}

void loop() {
  app.loop();
  
  if (app.ready()) {
    if (!userIdFetched) {
      getUserIdFromAuth();
      userIdFetched = true;
    }
    
    unsigned long currentTime = millis();
    
    if (currentTime - lastFetchTime >= fetchInterval && userId != "") {
      lastFetchTime = currentTime;
      Serial.println("\n================================");
      Serial.println("  Fetching latest data...");
      Serial.println("================================");
      fetchUserSubscriptionsFromFirebase();
      delay(2000);
      fetchCoinPrices();
    }
    
    if (currentTime - lastDisplayTime >= displayInterval) {
      lastDisplayTime = currentTime;
      displayStatus();
    }
  }
}

void getUserIdFromAuth() {
  Serial.println("Fetching authenticated user ID...");
  
  userId = app.getUid();
  
  if (userId != "") {
    Serial.print("User ID: ");
    Serial.println(userId);
  } else {
    Serial.println("Failed to get user ID, using hardcoded value");
    userId = "RZCeb75gvLWRyMECnUgqQI1xgRr2";
  }
}

void displayStatus() {
  Serial.println("\n================================");
  Serial.println("     CRYPTO ALERT SYSTEM");
  Serial.println("================================");
  Serial.print("User logged in: ");
  Serial.println(userEmail);
  if (userId != "") {
    Serial.print("User ID: ");
    Serial.println(userId);
  }
  Serial.println("\n--- Coins being monitored ---");
  
  if (subscriptionCount == 0) {
    Serial.println("  (No subscriptions found)");
  } else {
    for (int i = 0; i < subscriptionCount; i++) {
      if (subscriptions[i].hasData) {
        Serial.printf("  %s: $%.2f", 
                     subscriptions[i].symbol.c_str(), 
                     subscriptions[i].currentPrice);
        
        if (subscriptions[i].lower > 0 || subscriptions[i].upper > 0) {
          Serial.print(" [Alerts: ");
          if (subscriptions[i].lower > 0) {
            Serial.printf("Low<$%.2f", subscriptions[i].lower);
          }
          if (subscriptions[i].upper > 0) {
            if (subscriptions[i].lower > 0) Serial.print(", ");
            Serial.printf("High>$%.2f", subscriptions[i].upper);
          }
          Serial.print("]");
        }
        Serial.println();
      }
    }
  }
  Serial.println("--------------------------------");
}

void fetchUserSubscriptionsFromFirebase() {
  Serial.println("Fetching user subscriptions from Firebase...");
  
  String url = String(DATABASE_URL) + "users/" + userId + "/subscriptions.json";
  
  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure();
  http.begin(client, url);
  int httpCode = http.GET();
  
  subscriptionCount = 0;
  
  if (httpCode > 0) {
    String payload = http.getString();
    
    StaticJsonDocument<2048> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      JsonObject root = doc.as<JsonObject>();
      
      for (JsonPair kv : root) {
        String coinSymbol = String(kv.key().c_str());
        JsonObject coinData = kv.value().as<JsonObject>();
        
        subscriptions[subscriptionCount].symbol = coinSymbol;
        subscriptions[subscriptionCount].lower = coinData["lower"] | 0.0;
        subscriptions[subscriptionCount].upper = coinData["upper"] | 0.0;
        subscriptions[subscriptionCount].hasData = false;
        subscriptions[subscriptionCount].currentPrice = 0.0;
        
        Serial.printf("  Loaded: %s (lower: $%.2f, upper: $%.2f)\n", 
                     coinSymbol.c_str(), 
                     subscriptions[subscriptionCount].lower, 
                     subscriptions[subscriptionCount].upper);
        
        subscriptionCount++;
        if (subscriptionCount >= 20) break;
      }
      
      Serial.printf("Total subscriptions loaded: %d\n", subscriptionCount);
    } else {
      Serial.print("JSON parse error: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.printf("HTTP request failed, code: %d\n", httpCode);
  }
  
  http.end();
}

void fetchCoinPrices() {
  Serial.println("Fetching live coin prices...");
  
  upperAlerts = 0;
  lowerAlerts = 0;
  
  for (int i = 0; i < subscriptionCount; i++) {
    const String &symbol = subscriptions[i].symbol;
    String url = "https://api.coinlayer.com/api/live?access_key=" + String(COINLAYER_API_KEY) + "&symbols=" + symbol;
    
    WiFiClientSecure coinClient;
    coinClient.setInsecure();
    HTTPClient http;
    bool fetchedFromCoinLayer = false;
    if (http.begin(coinClient, url)) {
      int httpCode = http.GET();
      if (httpCode > 0) {
        String payload = http.getString();
        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (!error && doc["success"] == true) {
          float price = doc["rates"][symbol] | -1.0f;
          unsigned long timestamp = doc["timestamp"] | 0UL;
          if (timestamp == 0) {
            timestamp = (unsigned long)(millis() / 1000);
          }
          unsigned long historyKey = timestamp * 1000UL + (millis() % 1000);
          
          subscriptions[i].currentPrice = price;
          subscriptions[i].hasData = true;
          fetchedFromCoinLayer = true;
          
          Serial.printf("  %s: $%.2f (from CoinLayer)\n", symbol.c_str(), price);
          
          String basePath = "/coins/" + symbol;
          Database.set<float>(aClient, (basePath + "/price").c_str(), price, processData);
          Database.set<unsigned long>(aClient, (basePath + "/updatedAt").c_str(), timestamp, processData);
          Database.set<String>(aClient, (basePath + "/source").c_str(), String("esp32"), processData);

          String historyPath = basePath + "/history/" + String(historyKey);
          String historyData = "{\"price\":" + String(price) + ",\"timestamp\":" + String(timestamp) + "}";
          Database.set<String>(aClient, historyPath.c_str(), historyData, processData);
          
          checkAlertsAndTrigger(symbol, price, timestamp);
        }
      } else {
        Serial.printf("  CoinLayer HTTP error for %s: %d\n", symbol.c_str(), httpCode);
      }
      http.end();
    } else {
      Serial.printf("  Unable to open CoinLayer session for %s\n", symbol.c_str());
    }
    
    // If CoinLayer failed, try to read price from Firebase cache
    if (!fetchedFromCoinLayer) {
      Serial.printf("  CoinLayer failed for %s, checking Firebase...\n", symbol.c_str());
      String firebaseUrl = String(DATABASE_URL) + "coins/" + symbol + ".json";
      WiFiClientSecure cacheClient;
      cacheClient.setInsecure();
      HTTPClient httpFB;
      if (httpFB.begin(cacheClient, firebaseUrl)) {
        int fbHttpCode = httpFB.GET();
        if (fbHttpCode == 200) {
          String fbPayload = httpFB.getString();
          StaticJsonDocument<512> cacheDoc;
          DeserializationError cacheErr = deserializeJson(cacheDoc, fbPayload);
          if (!cacheErr) {
            float cachedPrice = cacheDoc["price"] | -1.0f;
            unsigned long cachedTimestamp = cacheDoc["updatedAt"] | 0UL;
            if (cachedPrice > 0) {
              subscriptions[i].currentPrice = cachedPrice;
              subscriptions[i].hasData = true;
              Serial.printf("  %s: $%.2f (from Firebase cache)\n", symbol.c_str(), cachedPrice);
              checkAlertsAndTrigger(symbol, cachedPrice, cachedTimestamp);
            } else {
              Serial.printf("  No cached price for %s\n", symbol.c_str());
            }
          } else {
            Serial.printf("  Cache parse error for %s: %s\n", symbol.c_str(), cacheErr.c_str());
          }
        } else {
          Serial.printf("  Firebase HTTP error for %s: %d\n", symbol.c_str(), fbHttpCode);
        }
        httpFB.end();
      } else {
        Serial.printf("  Unable to open Firebase cache session for %s\n", symbol.c_str());
      }
      if (!subscriptions[i].hasData) {
        Serial.printf("  ✗ No price data for %s\n", symbol.c_str());
      }
    }
    
    delay(500);
  }
  
  Serial.println("Price update complete");
  
  // Trigger combined alerts if any were detected
  if (upperAlerts > 0 || lowerAlerts > 0) {
    triggerAlerts();
  }
}

void checkAlertsAndTrigger(const String &symbol, float price, unsigned long epochSeconds) {
  for (int i = 0; i < subscriptionCount; i++) {
    if (subscriptions[i].symbol == symbol) {
      if (subscriptions[i].lower > 0 && price <= subscriptions[i].lower) {
        lowerAlerts++;
        Serial.println("\nALERT TRIGGERED!");
        Serial.printf("   %s dropped to $%.2f (below threshold: $%.2f)\n", 
                     symbol.c_str(), price, subscriptions[i].lower);
        logAlert(symbol, "LOWER", price, epochSeconds);
      } else if (subscriptions[i].upper > 0 && price >= subscriptions[i].upper) {
        upperAlerts++;
        Serial.println("\nALERT TRIGGERED!");
        Serial.printf("   %s rose to $%.2f (above threshold: $%.2f)\n", 
                     symbol.c_str(), price, subscriptions[i].upper);
        logAlert(symbol, "UPPER", price, epochSeconds);
      }
    }
  }
}

void logAlert(const String &symbol, const String &type, float price, unsigned long epochSeconds) {
  unsigned long alertTimestamp = epochSeconds > 0 ? epochSeconds : (unsigned long)(millis() / 1000);
  String alertPath = "/users/" + userId + "/alerts/" + String(alertTimestamp) + "_" + symbol + "_" + type;
  String alertData = "{\"symbol\":\"" + symbol + "\",\"type\":\"" + type +
                     "\",\"price\":" + String(price, 4) + ",\"timestamp\":" + String(alertTimestamp) + "}";
  Database.set<String>(aClient, alertPath.c_str(), alertData, processData);
}

void triggerAlerts() {
  // Determine LED states based on alert counts
  bool redState = (lowerAlerts > 0);
  bool greenState = (upperAlerts > 0);
  bool blueState = (upperAlerts > 1 || lowerAlerts > 1 || (upperAlerts > 0 && lowerAlerts > 0));
  
  Serial.println("\n[LED ALERT]");
  if (redState) Serial.println("   RED: Price(s) decreased");
  if (greenState) Serial.println("   GREEN: Price(s) increased");
  if (blueState) Serial.println("   BLUE: Multiple alerts");
  
  // Flash LEDs and buzzer 3 times
  for (int i = 0; i < 3; i++) {
    digitalWrite(RED_LED_PIN, redState ? HIGH : LOW);
    digitalWrite(GREEN_LED_PIN, greenState ? HIGH : LOW);
    digitalWrite(BLUE_LED_PIN, blueState ? HIGH : LOW);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(BLUE_LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);
  }
  
  Serial.println("   Alert sequence complete");
}

void processData(AsyncResult &aResult) {
  if (aResult.isError()) {
    Serial.printf("Firebase Error: %s\n", aResult.error().message().c_str());
  }
}
