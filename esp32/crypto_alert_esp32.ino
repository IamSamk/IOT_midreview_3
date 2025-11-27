#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Firebase_ESP_Client.h>

// ===== CREDENTIALS =====
#define WIFI_SSID "Not connected"
#define WIFI_PASSWORD "12345678"
#define API_KEY "AIzaSyAhQH-phbFZPui0itYuBdTBJIm1FpUsGRQ"
#define DATABASE_URL "https://iotcie3-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define USER_UID "RZCeb75gvLWRyMECnUgqQTlxgRr2"
#define USER_EMAIL "samarthkadambtech24@rvu.edu.in"
#define USER_PASSWORD "SamkRVU@24"
#define COINLAYER_API_KEY "f434d61712e69fdfbc3ac2a32d83b27f"
const char* coinlayerEndpoint = "http://api.coinlayer.com/api/live?access_key=" COINLAYER_API_KEY;

// ==== HARDWARE SETUP =====
LiquidCrystal_I2C lcd(0x27, 20, 4);
#define SS_PIN   5
#define RST_PIN  4
MFRC522 rfid(SS_PIN, RST_PIN);
const byte ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {
    {'1','2','3','A'}, {'4','5','6','B'},
    {'7','8','9','C'}, {'*','0','#','D'}
};
byte rowPins[ROWS] = {32, 33, 25, 26};
byte colPins[COLS] = {27, 14, 12, 13};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ==== STATE TYPES ====
enum State {
  STATE_WAIT_CARD, STATE_WELCOME,
  STATE_MAIN_MENU, STATE_COINS, STATE_COIN_DETAIL, STATE_SET_LIMIT,
  STATE_CHOOSE_COIN, STATE_KEYMAP, STATE_LOGOUT_CONFIRM, STATE_ALERT
};
State menuState = STATE_WAIT_CARD;
unsigned long lastCardSignalTime = 0;
bool inWaitForCard = true; // start in wait state
// ==== COIN STRUCTURES & BUFFERS ====
struct Coin {
  String symbol;
  float price;
  float lower, upper;
  bool subscribed;
  bool triggered;
  bool lowerLatched;  // NEW
  bool upperLatched;  // NEW
};
Coin simCoins[3] = {
    {"coin1", 32000, 0, 0, false, false, false, false},
    {"coin2", 2100, 0, 0, false, false, false, false},
    {"coin3", 0.65, 0, 0, false, false, false, false}
};
const int simCount = 3;
unsigned long lastCoin1Time = 0, lastCoin2Time = 0, lastCoin3Time = 0;
bool coin1Up = true, coin2Up = true, coin3Up = true;
struct MarketCoin {
  String symbol;
  float price;
  bool subscribed;
  float lower, upper;
  bool triggered;
  bool lowerLatched;  // NEW
  bool upperLatched;  // NEW
};
MarketCoin marketCoins[10];
const char* keyCoins[10] = { "BTC","ETH","DOGE","LTC","BCH","BNB","XRP","ADA","DOT","SOL" };
int marketCount = 0;

// ==== USER + MENU BUFFERS ====
String currentUID = "", currentName = "", currentID = "", currentEmail = "";
bool loggedIn = false;
int menuCursor = 0, menuScroll = 0, selectedCoin = 0;
String limitInput = ""; bool settingUpper = false, settingLower = false;
int alertCount = 0; String alertMsg[4];
unsigned long lastSimTime = 0, lastCoinFetch = 0, lastSimDbUpdate = 0;

// ==== UTILITY: NETWORK & INIT ====
void lcdStepMsg(const String &msg) { lcd.clear(); lcd.setCursor(0,1); lcd.print(msg); }
void networkConnect() {
  lcdStepMsg("Connecting WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries++ < 50) {
    delay(250); lcd.setCursor((tries%18),2); lcd.print(".");
  }
  lcdStepMsg(WiFi.status() == WL_CONNECTED ? "WiFi OK" : "WiFi Failed");
  delay(500);
}
void firebaseConnect() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL; auth.user.password = USER_PASSWORD;
  lcdStepMsg("Connecting Firebase");
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  int tries = 0;
  while ((config.signer.tokens.status != token_status_ready) && tries++ < 40) {
    lcd.setCursor((tries%18),2); lcd.print("."); delay(200);
  }
  lcdStepMsg(config.signer.tokens.status == token_status_ready ? "Firebase OK" : "FB failed!");
  delay(600);
}
void fetchCoinsFromAPI() {
  lcdStepMsg("Crypto Market...");
  marketCount = 0;
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.begin(String(coinlayerEndpoint));
  int code = http.GET();
  if (code == 200) {
    String resp = http.getString();
    DynamicJsonDocument doc(1600); deserializeJson(doc, resp);
    JsonObject rates = doc["rates"];
    for (int i=0;i<10;i++) {
      String sym = keyCoins[i];
      if (rates.containsKey(sym)) {
        marketCoins[marketCount].symbol = sym;
        marketCoins[marketCount].price = rates[sym].as<float>();
        marketCoins[marketCount].subscribed = false;
        marketCoins[marketCount].lower = 0;
        marketCoins[marketCount].upper = 0;
        marketCoins[marketCount].triggered = false;
        marketCount++;
      }
    }
    lcdStepMsg("Market OK"); lcd.setCursor(0,2); lcd.print("BTC: "); lcd.print(marketCoins[0].price,2); delay(1200);
    lcd.clear(); lcd.setCursor(2,1); lcd.print("Tap card to login");
  } else {
    lcdStepMsg("Market FAIL!"); delay(1200);
  }
  http.end();
  lastCoinFetch = millis();
}

// More: Firebase user handling, menu rendering, scroll logic, menu state machine, next prompt!

// ==== USER DATA LOAD/SAVE ====
struct UserCoinPrefs {
  bool subs[13];
  float lowers[13];
  float uppers[13];
};
UserCoinPrefs userPrefs;

void loadUserCoinPrefs() {
  String path = "/users/" USER_UID "/subscriptions";
  if (Firebase.RTDB.getJSON(&fbdo, path)) {
    FirebaseJson &json = fbdo.jsonObject();
    for (int i = 0; i < simCount; i++) {
      String key = simCoins[i].symbol;
      FirebaseJsonData result;
      json.get(result, key + "/on"); userPrefs.subs[i] = result.boolValue;
      json.get(result, key + "/lower"); userPrefs.lowers[i] = result.to<float>();
      json.get(result, key + "/upper"); userPrefs.uppers[i] = result.to<float>();
      simCoins[i].subscribed = userPrefs.subs[i];
      simCoins[i].lower = userPrefs.lowers[i];
      simCoins[i].upper = userPrefs.uppers[i];
    }
    for (int i = 0; i < marketCount; i++) {
      String key = marketCoins[i].symbol;
      FirebaseJsonData result;
      json.get(result, key + "/on"); userPrefs.subs[simCount + i] = result.boolValue;
      json.get(result, key + "/lower"); userPrefs.lowers[simCount + i] = result.to<float>();
      json.get(result, key + "/upper"); userPrefs.uppers[simCount + i] = result.to<float>();
      marketCoins[i].subscribed = userPrefs.subs[simCount + i];
      marketCoins[i].lower = userPrefs.lowers[simCount + i];
      marketCoins[i].upper = userPrefs.uppers[simCount + i];
    }
  }
}
void saveUserCoinPref(int idx, bool isSim) {
  String base = "/users/" USER_UID "/subscriptions/";
  String key = isSim ? simCoins[idx].symbol : marketCoins[idx].symbol;
  String path = base + key;
  FirebaseJson json;
  bool ison = isSim ? simCoins[idx].subscribed : marketCoins[idx].subscribed;
  float l = isSim ? simCoins[idx].lower : marketCoins[idx].lower;
  float u = isSim ? simCoins[idx].upper : marketCoins[idx].upper;
  json.set("on", ison);
  json.set("lower", l);
  json.set("upper", u);
  Firebase.RTDB.setJSON(&fbdo, path, &json);
}
void updateSimCoinsToCloud() {
  String root = "/coins/";
  for (int i = 0; i < simCount; i++) {
    String path = root + simCoins[i].symbol;
    FirebaseJson json; json.set("price", simCoins[i].price);
    Firebase.RTDB.setJSON(&fbdo, path, &json);
  }
}

// ==== FILTERED SCROLL/INDEX BUFFER FOR ROBUST PAGING ====
int visibleIndices[16]; // max 16 for safety. [index in sim/market arrays]
int visibleTypes[16];   // 0=sim, 1=market
int numVisible;         // entries in visibleIndices[]

void buildCurrentCoinsBuffer() {
  numVisible = 0;
  for (int i = 0; i < simCount; ++i)
    if (simCoins[i].subscribed) { visibleIndices[numVisible]=i; visibleTypes[numVisible]=0; numVisible++; }
  for (int i = 0; i < marketCount; ++i)
    if (marketCoins[i].subscribed) { visibleIndices[numVisible]=i; visibleTypes[numVisible]=1; numVisible++; }
}

void buildChooseCoinsBuffer() {
  numVisible = 0;
  // Only **non-subscribed** simulated coins
  for (int i = 0; i < simCount; ++i) {
    if (!simCoins[i].subscribed) {
      visibleIndices[numVisible]=i; visibleTypes[numVisible]=0; numVisible++;
    }
  }
  // Only non-subscribed (or not-in-database) market coins
  for (int i = 0; i < marketCount; ++i) {
    if (!marketCoins[i].subscribed) {
      visibleIndices[numVisible]=i; visibleTypes[numVisible]=1; numVisible++;
    }
  }
}


// ==== MENU RENDERING ====
void drawMainMenu() {
  const char* labels[3] = { "Current coins/Limits", "Choose coins", "Key mappings" };
  lcd.clear();
  for (int i = 0; i < 3; i++) {
    lcd.setCursor(0, i);
    lcd.print(i == menuCursor ? ">" : " ");
    lcd.print(labels[i]);
  }
  lcd.setCursor(0, 3); lcd.print("A:Up B:Down *:Sel");
}
void drawCoinsMenu() {
  buildCurrentCoinsBuffer();
  lcd.clear();
  for (int row = 0; row < 4; ++row) {
    int idx = menuScroll + row;
    if (idx >= numVisible) break;
    int coinIdx = visibleIndices[idx];
    int t = visibleTypes[idx];
    lcd.setCursor(0, row);
    lcd.print((row == menuCursor) ? ">" : " ");
    if (t==0)
      lcd.print(simCoins[coinIdx].symbol + " - " + String(simCoins[coinIdx].price,2));
    else
      lcd.print(marketCoins[coinIdx].symbol + " - " + String(marketCoins[coinIdx].price,2));
  }
  lcd.setCursor(0,3); lcd.print("A/B=Scroll *:Det #");
}
void drawChooseCoins() {
  buildChooseCoinsBuffer();
  lcd.clear();
  for (int row = 0; row < 4; ++row) {
    int idx = menuScroll + row;
    if (idx >= numVisible) break;
    int coinIdx = visibleIndices[idx];
    int t = visibleTypes[idx];
    lcd.setCursor(0, row);
    lcd.print((row == menuCursor) ? ">" : " ");
    if (t==0)
      lcd.print(simCoins[coinIdx].symbol + " - " + String(simCoins[coinIdx].price,2) +
                (simCoins[coinIdx].subscribed ? " ON" : " OFF"));
    else
      lcd.print(marketCoins[coinIdx].symbol + " - " + String(marketCoins[coinIdx].price,2) + " OFF");
  }
  lcd.setCursor(0,3); lcd.print("*=Toggle #=Back");
}

// (Next reply: menu navigation, alert handling and state machine!)

void drawCoinDetail(int idx) {
  lcd.clear();
  int arrIdx = menuScroll + idx;
  if (arrIdx >= numVisible) return;
  int coinIdx = visibleIndices[arrIdx];
  int t      = visibleTypes[arrIdx];
  Coin* c = (t==0) ? &simCoins[coinIdx] : (Coin*)&marketCoins[coinIdx];
  lcd.setCursor(0,0); lcd.print(c->symbol + " Details");
  const char* opts[3] = { "Remove","Lower Limit", "Upper Limit" };
  for (int i = 0; i < 3; i++) {
    lcd.setCursor(0,i+1);
    lcd.print(i == menuCursor ? " >" : "  ");
    lcd.print(opts[i]);
    if (i == 1 && c->lower > 0) { lcd.print(" ("); lcd.print(c->lower,2); lcd.print(")"); }
    if (i == 2 && c->upper > 0) { lcd.print(" ("); lcd.print(c->upper,2); lcd.print(")"); }
  }
}
void drawLimitInput(String label) {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print(label);
  lcd.setCursor(0,3); lcd.print("#=Dot  *=Enter");
  lcd.setCursor(0,1); lcd.print(limitInput);
}
void drawAlert() {
  lcd.clear();
  for (int i = 0; i < alertCount && i < 4; i++) {
    lcd.setCursor(0, i);
    String msg = "Alert!! - " + alertMsg[i];
    if(msg.length() > 19) msg = msg.substring(0,19);
    lcd.print(msg);
  }
  lcd.setCursor(0,3); lcd.print("ALERT! Press key...");
}

// ==== ALERT LOGIC ====
void checkAlerts() {
  alertCount = 0;
  
  // Check simulated coins
  for (int i = 0; i < simCount; i++) {
    simCoins[i].triggered = false;
    if (simCoins[i].subscribed) {
      // Lower threshold
      if (simCoins[i].lower > 0 && simCoins[i].price < simCoins[i].lower) {
        if (!simCoins[i].lowerLatched) {  // Only trigger once
          simCoins[i].triggered = true;
          simCoins[i].lowerLatched = true;
          alertMsg[alertCount++] = simCoins[i].symbol + " <L " + String(simCoins[i].lower,2);
        }
      } else if (simCoins[i].price >= simCoins[i].lower) {
        simCoins[i].lowerLatched = false;  // Reset when price recovers
      }
      
      // Upper threshold
      if (simCoins[i].upper > 0 && simCoins[i].price > simCoins[i].upper) {
        if (!simCoins[i].upperLatched) {
          simCoins[i].triggered = true;
          simCoins[i].upperLatched = true;
          alertMsg[alertCount++] = simCoins[i].symbol + " >U " + String(simCoins[i].upper,2);
        }
      } else if (simCoins[i].price <= simCoins[i].upper) {
        simCoins[i].upperLatched = false;
      }
      
      if (alertCount == 4) break;
    }
  }
  
  // Check market coins
  for (int i = 0; i < marketCount; i++) {
    marketCoins[i].triggered = false;
    if (marketCoins[i].subscribed) {
      if (marketCoins[i].lower > 0 && marketCoins[i].price < marketCoins[i].lower) {
        if (!marketCoins[i].lowerLatched) {
          marketCoins[i].triggered = true;
          marketCoins[i].lowerLatched = true;
          alertMsg[alertCount++] = marketCoins[i].symbol + " <L " + String(marketCoins[i].lower,2);
        }
      } else if (marketCoins[i].price >= marketCoins[i].lower) {
        marketCoins[i].lowerLatched = false;
      }
      
      if (marketCoins[i].upper > 0 && marketCoins[i].price > marketCoins[i].upper) {
        if (!marketCoins[i].upperLatched) {
          marketCoins[i].triggered = true;
          marketCoins[i].upperLatched = true;
          alertMsg[alertCount++] = marketCoins[i].symbol + " >U " + String(marketCoins[i].upper,2);
        }
      } else if (marketCoins[i].price <= marketCoins[i].upper) {
        marketCoins[i].upperLatched = false;
      }
      
      if (alertCount == 4) break;
    }
  }
  
  if (alertCount > 0 && menuState != STATE_ALERT) {
    menuState = STATE_ALERT;
    drawAlert();
  }
}

// ==== MAIN MENU NAVIGATION HANDLER ====
void handleMenu(char key) {
  switch(menuState) {
    case STATE_WAIT_CARD: break;
    case STATE_WELCOME:   menuState = STATE_MAIN_MENU; menuCursor = 0; drawMainMenu(); break;

    case STATE_MAIN_MENU:
      if(key=='A'&&menuCursor>0){menuCursor--;drawMainMenu();}
      else if(key=='B'&&menuCursor<2){menuCursor++;drawMainMenu();}
      else if(key=='*'){
        if(menuCursor==0){menuState=STATE_COINS;menuCursor=0;menuScroll=0;drawCoinsMenu();}
        else if(menuCursor==1){menuState=STATE_CHOOSE_COIN;menuCursor=0;menuScroll=0;drawChooseCoins();}
        else{menuState=STATE_KEYMAP;drawKeyMapping();}
      }
      break;

    case STATE_COINS: {
      buildCurrentCoinsBuffer();
      if(numVisible==0) { menuCursor=0; menuScroll=0; drawCoinsMenu(); break;}
      if(key=='A'){
        if(menuCursor>0){menuCursor--;}
        else if(menuScroll>0) {menuScroll--;}
        drawCoinsMenu();
      }
      else if(key=='B'){
        int shown = (numVisible-menuScroll >= 4) ? 3 : (numVisible-menuScroll-1);
        if(menuCursor<shown){menuCursor++;}
        else if(menuScroll+4<numVisible){menuScroll++;}
        drawCoinsMenu();
      }
      else if(key=='*'){
        selectedCoin = menuScroll + menuCursor;
        menuState = STATE_COIN_DETAIL; menuCursor=0; drawCoinDetail(menuCursor);
      }
      else if(key=='#'){menuState=STATE_MAIN_MENU;menuCursor=0;drawMainMenu();}
      break;
    }
    case STATE_COIN_DETAIL: {
      int arrIdx = menuScroll + selectedCoin;
      if (arrIdx >= numVisible) break;
      Coin* c = (visibleTypes[arrIdx]==0) ? &simCoins[visibleIndices[arrIdx]] : (Coin*)&marketCoins[visibleIndices[arrIdx]];
      if(key=='A'&&menuCursor>0){menuCursor--; drawCoinDetail(selectedCoin);}
      else if(key=='B'&&menuCursor<2){menuCursor++; drawCoinDetail(selectedCoin);}
      else if(key=='*'){
        if(menuCursor==0){c->subscribed=false;saveUserCoinPref(visibleIndices[arrIdx],visibleTypes[arrIdx]==0);}
        else if(menuCursor==1){menuState=STATE_SET_LIMIT;limitInput="";settingLower=true;settingUpper=false;drawLimitInput("Set Lower Limit:");}
        else{menuState=STATE_SET_LIMIT;limitInput="";settingLower=false;settingUpper=true;drawLimitInput("Set Upper Limit:");}
      }
      else if (key == '#'){menuState=STATE_COINS;menuCursor=0;drawCoinsMenu();}
      break;
    }
    case STATE_SET_LIMIT: {
      int arrIdx = menuScroll + selectedCoin;
      Coin* c = (visibleTypes[arrIdx]==0) ? &simCoins[visibleIndices[arrIdx]] : (Coin*)&marketCoins[visibleIndices[arrIdx]];
      if ((key >= '0' && key <= '9') || key == '#') {
        if (limitInput.length() < 8) {
          limitInput += (key == '#') ? "." : String(key);
          drawLimitInput(settingLower ? "Set Lower Limit:" : "Set Upper Limit:");
        }
      }
      else if (key == '*') {
        if (settingLower) c->lower = limitInput.toFloat();
        else if (settingUpper) c->upper = limitInput.toFloat();
        saveUserCoinPref(visibleIndices[arrIdx],visibleTypes[arrIdx]==0);
        settingLower = settingUpper = false; limitInput = "";
        menuState=STATE_COIN_DETAIL;menuCursor=0;drawCoinDetail(selectedCoin);
      }
      else if (key == '#') {
        menuState = STATE_COIN_DETAIL; menuCursor = 0; drawCoinDetail(selectedCoin);
      }
      break;
    }
    case STATE_CHOOSE_COIN: {
      buildChooseCoinsBuffer();
      if(numVisible==0) {menuCursor=0;menuScroll=0;drawChooseCoins(); break;}
      if(key=='A'){
        if(menuCursor>0){menuCursor--;}
        else if(menuScroll>0){menuScroll--;}
        drawChooseCoins();
      }
      else if(key=='B'){
        int shown = (numVisible-menuScroll >= 4) ? 3 : (numVisible-menuScroll-1);
        if(menuCursor<shown){menuCursor++;}
        else if(menuScroll+4<numVisible){menuScroll++;}
        drawChooseCoins();
      }
      else if(key=='*'){
        int arrIdx = menuScroll + menuCursor;
        if(visibleTypes[arrIdx]==0)
          simCoins[visibleIndices[arrIdx]].subscribed = !simCoins[visibleIndices[arrIdx]].subscribed;
        else
          marketCoins[visibleIndices[arrIdx]].subscribed = !marketCoins[visibleIndices[arrIdx]].subscribed;
        saveUserCoinPref(visibleIndices[arrIdx], visibleTypes[arrIdx]==0);
        buildChooseCoinsBuffer();
        drawChooseCoins();
      }
      else if(key=='#'){menuState=STATE_MAIN_MENU;menuCursor=0;drawMainMenu();}
      break;
    }
    case STATE_KEYMAP: if(key=='*'||key=='#'){menuState=STATE_MAIN_MENU;menuCursor=0;drawMainMenu();} break;
    case STATE_LOGOUT_CONFIRM:
      if((key=='A'||key=='B')){menuCursor=1-menuCursor;drawLogoutConfirm();}
      else if(key=='*'){
        if(menuCursor==0){
          loggedIn=false;currentID="";currentUID="";currentName="";menuState=STATE_WAIT_CARD;
          lcd.clear();lcd.setCursor(2,0);lcd.print("RFID Login System");lcd.setCursor(0,2);lcd.print("Please tap card...");
        }
        else {menuState=STATE_MAIN_MENU;menuCursor=0;drawMainMenu();}
      }
      break;
    case STATE_ALERT:
    // User acknowledged alerts – clear them
    alertCount = 0;
    for (int i = 0; i < simCount; i++) simCoins[i].triggered = false;
    for (int i = 0; i < marketCount; i++) marketCoins[i].triggered = false;

    menuState = STATE_MAIN_MENU;
    menuCursor = 0;
    drawMainMenu();
    break;

  }
}
void fetchSimCoinsFromFirebase() {
    for (int i = 0; i < 3; i++) {
        String path = "/coins/" + simCoins[i].symbol + "/price";
        if (Firebase.RTDB.getFloat(&fbdo, path)) {
            simCoins[i].price = fbdo.floatData();
        }
    }
}


// ==== MAIN SETUP/LOOP ====
void setup() {
  Serial.begin(115200);
  Wire.begin(21,22); lcd.init(); lcd.backlight();
  SPI.begin(); rfid.PCD_Init();
  
  // Initialize latch flags
  for (int i = 0; i < simCount; i++) {
    simCoins[i].lowerLatched = false;
    simCoins[i].upperLatched = false;
  }
  
  lcd.clear(); lcd.setCursor(2,0); lcd.print("RFID Login System");
  lcd.setCursor(0,2); lcd.print("Please tap card...");
  networkConnect();
  firebaseConnect();
  fetchCoinsFromAPI();    // fetch market coins
  loadUserCoinPrefs();    // load user coin prefs
  lastSimTime = millis(); lastSimDbUpdate = millis(); lastCoinFetch = millis();
  inWaitForCard = true;
  lastCardSignalTime = millis();
}

void loop() {
static unsigned long lastSimFetch = 0;
if (millis() - lastSimFetch > 5000UL) {
    lastSimFetch = millis();
    fetchSimCoinsFromFirebase();
}
 // If logged in and no card activity for 5 seconds, go back to wait card screen

  // CoinLayer API market update every 1 minute
  if(millis()-lastCoinFetch>60000UL) fetchCoinsFromAPI();
  // RFID login events (always ready)
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    String uid = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      uid += (rfid.uid.uidByte[i]<0x10 ? "0" : ""); 
      uid += String(rfid.uid.uidByte[i], HEX); 
      if (i<rfid.uid.size-1) uid += " ";
    }
    uid.toUpperCase();

currentUID = uid;
currentName = "";
currentID = "";
currentEmail = "";
loggedIn = false;
// any successful read is card activity
    lastCardSignalTime = millis();
    inWaitForCard = false;
if (uid == "13 BA 90 22") {  // Locked card, use UID only
    currentName = "Samarth";
    currentID = "1RUA24CSE399";
    currentEmail = USER_EMAIL;
    loggedIn = true;
    // Don't call loadUserCoinPrefs(); memory read blocked on this card
} else if (uid == "03 84 C6 2A") {  // Default key card — full access
    currentName = "Samarth";
    currentID = "1RUA24CSE399";
    currentEmail = USER_EMAIL;
    loggedIn = true;
    loadUserCoinPrefs();  // Read preferences from card/db
}

    lcd.clear();
    if(!loggedIn){
      lcd.setCursor(0,0);lcd.print("Unknown Card");lcd.setCursor(0,1);lcd.print("UID:");lcd.setCursor(0,2);lcd.print(currentUID);
      lcd.setCursor(0,3);lcd.print("Access Denied!");
      delay(1750);lcd.clear();lcd.setCursor(0,1);lcd.print("Please tap card...");
      menuState=STATE_WAIT_CARD;
      inWaitForCard = true;     
    } else if(menuState!=STATE_WAIT_CARD){
      menuState=STATE_LOGOUT_CONFIRM;menuCursor=0;drawLogoutConfirm();
    } else{
      menuState=STATE_WELCOME; lcd.clear();
      lcd.setCursor(0,0); lcd.print("Welcome " + currentName);
      lcd.setCursor(0,2); lcd.print(">Press any key...");
    }
    rfid.PICC_HaltA();rfid.PCD_StopCrypto1();delay(800);return;
  }
  
  // Only check alerts when not on ALERT screen itself
  if (menuState != STATE_WAIT_CARD &&
      menuState != STATE_WELCOME &&
      menuState != STATE_ALERT) {
    checkAlerts();
  }


  // Keypad/menu
  char key = keypad.getKey();
  if (key) handleMenu(key);
  delay(5);
  }
void drawKeyMapping() {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Keypad Mappings:");
  lcd.setCursor(0,1); lcd.print("A:Up  B:Down");
  lcd.setCursor(0,2); lcd.print("C:Left D:Right");
  lcd.setCursor(0,3); lcd.print("#:Dot *=Enter *+#:Back");
}
void drawLogoutConfirm() {
  lcd.clear();
  lcd.setCursor(0,1); lcd.print("Logout & save info?");
  lcd.setCursor(2,3); lcd.print(menuCursor==0?">Yes     No":" Yes    >No");
}
