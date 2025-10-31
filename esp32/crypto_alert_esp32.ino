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

// Hardware pins - YOUR CONFIGURATION
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
const unsigned long fetchInterval = 60000; // Fetch every 60 seconds
const unsigned long displayInterval = 5000; // Display status every 5 seconds

// User ID
String userId = "RZCeb75gvLWRyMECnUgqQI1xgRr2";
String userEmail = USER_EMAIL;

// Store subscription data
struct Subscription {
  String symbol;
  float lower;
  float upper;
  float currentPrice;
  bool hasData;
};

Subscription subscriptions[10];
int subscriptionCount = 0;

// Alert tracking
int alertsTriggeredCount = 0;

void processData(AsyncResult &aResult);
void fetchCoinPrices();
void fetchUserSubscriptions();
void checkAlertsAndTrigger(String symbol, float price);
void triggerAlert(String symbol, String type, int totalAlerts);
void displayStatus();

void setup() {
  Serial.begin(115200);
  
  // Setup hardware pins
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
  Serial.println("\n✓ WiFi connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  
  ssl_client.setInsecure();
  ssl_client.setConnectionTimeout(1000);
  ssl_client.setHandshakeTimeout(5);
  
  initializeApp(aClient, app, getAuth(user_auth), processData, "authTask");
  app.getApp<RealtimeDatabase>(Database);
  Database.url(DATABASE_URL);
  
  Serial.println("✓ Firebase initialized!");
  Serial.println("✓ Authenticating user...");
}

void loop() {
  app.loop();
  
  if (app.ready()) {
    unsigned long currentTime = millis();
    
    // Fetch coin prices and user subscriptions periodically
    if (currentTime - lastFetchTime >= fetchInterval) {
      lastFetchTime = currentTime;
      Serial.println("\n════════════════════════════════");
      Serial.println("  Fetching latest data...");
      Serial.println("════════════════════════════════");
      fetchUserSubscriptions();
      delay(1000);
      fetchCoinPrices();
    }
    
    // Display status every 5 seconds
    if (currentTime - lastDisplayTime >= displayInterval) {
      lastDisplayTime = currentTime;
      displayStatus();
    }
  }
}

void displayStatus() {
  Serial.println("\n╔════════════════════════════════╗");
  Serial.println("║     CRYPTO ALERT SYSTEM        ║");
  Serial.println("╚════════════════════════════════╝");
  Serial.print("User logged in: ");
  Serial.println(userEmail);
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
  Serial.println("────────────────────────────────");
}

void fetchUserSubscriptions() {
  Serial.println("→ Loading user subscriptions from Firebase...");
  
  String coins[] = {"BTC", "APC", "APPC"};
  subscriptionCount = 3;
  
  for (int i = 0; i < subscriptionCount; i++) {
    subscriptions[i].symbol = coins[i];
    subscriptions[i].hasData = false;
    
    // For demo, use hardcoded thresholds
    // In production, you'd fetch these from Firebase in the processData callback
    subscriptions[i].lower = 0;   // Set your lower threshold here
    subscriptions[i].upper = 0;   // Set your upper threshold here
    
    // Example: Set thresholds for BTC
    if (coins[i] == "BTC") {
      subscriptions[i].lower = 60000;  // Alert if BTC drops below $60,000
      subscriptions[i].upper = 70000;  // Alert if BTC rises above $70,000
    }
  }
  
  Serial.println("✓ Subscriptions loaded");
}

void fetchCoinPrices() {
  Serial.println("→ Fetching live coin prices from CoinLayer...");
  
  // Reset alert counter
  alertsTriggeredCount = 0;
  
  for (int i = 0; i < subscriptionCount; i++) {
    String url = "https://api.coinlayer.com/api/live?access_key=" + 
                 String(COINLAYER_API_KEY) + "&symbols=" + subscriptions[i].symbol;
    
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode > 0) {
      String payload = http.getString();
      
      StaticJsonDocument<1024> doc;
      DeserializationError error = deserializeJson(doc, payload);
      
      if (!error && doc["success"] == true) {
        float price = doc["rates"][subscriptions[i].symbol];
        unsigned long timestamp = millis();
        
        subscriptions[i].currentPrice = price;
        subscriptions[i].hasData = true;
        
        Serial.printf("  ✓ %s: $%.2f\n", subscriptions[i].symbol.c_str(), price);
        
        // Update Firebase with price and timestamp
        String pricePath = "/coins/" + subscriptions[i].symbol + "/price";
        String timePath = "/coins/" + subscriptions[i].symbol + "/updatedAt";
        
        Database.set<float>(aClient, pricePath.c_str(), price, processData);
        Database.set<unsigned long>(aClient, timePath.c_str(), timestamp, processData);
        
        // Log to history for charting
        String historyPath = "/coins/" + subscriptions[i].symbol + "/history/" + String(timestamp);
        String historyData = "{\"price\":" + String(price) + ",\"timestamp\":" + String(timestamp) + "}";
        Database.set<String>(aClient, historyPath.c_str(), historyData, processData);
        
        // Check alerts
        checkAlertsAndTrigger(subscriptions[i].symbol, price);
      } else {
        Serial.printf("  ✗ Failed to fetch %s\n", subscriptions[i].symbol.c_str());
      }
    }
    http.end();
    delay(500);
  }
  
  Serial.println("✓ Price update complete");
}

void checkAlertsAndTrigger(String symbol, float price) {
  for (int i = 0; i < subscriptionCount; i++) {
    if (subscriptions[i].symbol == symbol) {
      if (subscriptions[i].lower > 0 && price <= subscriptions[i].lower) {
        alertsTriggeredCount++;
        Serial.println("\n🚨 ALERT TRIGGERED!");
        Serial.printf("   %s dropped to $%.2f (below threshold: $%.2f)\n", 
                     symbol.c_str(), price, subscriptions[i].lower);
        triggerAlert(symbol, "LOWER", alertsTriggeredCount);
      }
      else if (subscriptions[i].upper > 0 && price >= subscriptions[i].upper) {
        alertsTriggeredCount++;
        Serial.println("\n🚨 ALERT TRIGGERED!");
        Serial.printf("   %s rose to $%.2f (above threshold: $%.2f)\n", 
                     symbol.c_str(), price, subscriptions[i].upper);
        triggerAlert(symbol, "UPPER", alertsTriggeredCount);
      }
    }
  }
}

void triggerAlert(String symbol, String type, int totalAlerts) {
  // Determine LED color based on alert type and count
  int redState = LOW;
  int greenState = LOW;
  int blueState = LOW;
  
  if (totalAlerts > 1) {
    // Multiple alerts - BLUE LED
    blueState = HIGH;
    Serial.println("   [LED: BLUE - Multiple alerts]");
  } else {
    // Single alert - color based on type
    if (type == "UPPER") {
      // Price increased - GREEN LED
      greenState = HIGH;
      Serial.println("   [LED: GREEN - Price increased]");
    } else {
      // Price decreased - RED LED
      redState = HIGH;
      Serial.println("   [LED: RED - Price decreased]");
    }
  }
  
  // Activate LED and buzzer (3 beeps)
  for (int i = 0; i < 3; i++) {
    digitalWrite(RED_LED_PIN, redState);
    digitalWrite(GREEN_LED_PIN, greenState);
    digitalWrite(BLUE_LED_PIN, blueState);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(BLUE_LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    delay(200);
  }
  
  // Log alert to Firebase
  String alertPath = "/users/" + userId + "/alerts/" + String(millis());
  String alertData = "{\"symbol\":\"" + symbol + "\",\"type\":\"" + type + 
                     "\",\"timestamp\":" + String(millis()) + "}";
  
  Database.set<String>(aClient, alertPath.c_str(), alertData, processData);
  Serial.println("   Alert logged to Firebase");
}

void processData(AsyncResult &aResult) {
  if (!aResult.isResult())
    return;

  if (aResult.isError())
    Serial.printf("Firebase Error: %s\n", aResult.error().message().c_str());
}
