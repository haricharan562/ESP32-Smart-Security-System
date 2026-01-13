#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <HTTPClient.h>

/* ================= OLED ================= */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

/* ================= PINS ================= */
#define PIR_PIN     33
#define BUTTON_PIN  25
#define BUZZER_PIN  14

/* ================= SYSTEM STATES =================
   IDLE  : System disarmed
   ARMED : System armed, monitoring motion
   ALERT : Motion detected, buzzer active
*/
enum SystemState { IDLE, ARMED, ALERT };
SystemState currentState = IDLE;

/* ================= BUTTON ================= */
bool lastButtonState = HIGH;
bool buttonState;

/* ================= TIMING ================= */
unsigned long alertStartTime = 0;
const unsigned long alertDuration = 5000;   // 5 seconds buzzer

unsigned long lastThingSpeakUpdate = 0;
const unsigned long thingSpeakInterval = 15000; // 15 sec (ThingSpeak limit)

/* ================= WIFI / THINGSPEAK ================= */
const char* ssid = "HARI'S PHONE";
const char* password = "harisphone";
const char* thingSpeakAPIKey = "21TCNJ5L6MNA1DPC";

bool wifiConnected = false;

/* ================= SETUP ================= */
void setup() {
  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  Wire.begin(21, 22);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found");
    while (true);
  }

  displayMessage("System Ready", "IDLE MODE");
  connectWiFi();
}

/* ================= LOOP ================= */
void loop() {
  handleButton();
  handleSystem();
}

/* ================= WIFI CONNECT ================= */
void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - startTime < 5000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("\nWiFi Connected");
    Serial.println(WiFi.localIP());
  } else {
    wifiConnected = false;
    Serial.println("\nWiFi not connected. Running offline.");
  }
}

/* ================= BUTTON LOGIC =================
   Button toggles:
   IDLE  <-->  ARMED
*/
void handleButton() {
  buttonState = digitalRead(BUTTON_PIN);

  if (buttonState == LOW && lastButtonState == HIGH) {
    if (currentState == IDLE) {
      currentState = ARMED;
    } else {
      currentState = IDLE;
      noTone(BUZZER_PIN);
    }
    delay(200); // debounce
  }
  lastButtonState = buttonState;
}

/* ================= SYSTEM STATE MACHINE ================= */
void handleSystem() {
  bool motionDetected = digitalRead(PIR_PIN);

  switch (currentState) {

    case IDLE:
      noTone(BUZZER_PIN);
      displayMessage("System Status", "IDLE MODE");
      sendToThingSpeak(0, 0);   // Field1=0, Field2=0
      break;

    case ARMED:
      noTone(BUZZER_PIN);
      displayMessage("System Status", "ARMED");
      sendToThingSpeak(1, 0);   // Field1=1, Field2=0

      if (motionDetected) {
        currentState = ALERT;
        alertStartTime = millis();
      }
      break;

    case ALERT:
      tone(BUZZER_PIN, 1000);
      displayMessage("!!! ALERT !!!", "Motion Detected");
      sendToThingSpeak(1, 1);   // Field1=1, Field2=1

      if (millis() - alertStartTime > alertDuration) {
        noTone(BUZZER_PIN);
        currentState = ARMED;
      }
      break;
  }
}

/* ================= OLED DISPLAY ================= */
void displayMessage(String line1, String line2) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 15);
  display.println(line1);

  display.setCursor(0, 35);
  display.println(line2);

  display.display();
}

/* ================= THINGSPEAK =================
   FIELD MAPPING (VERY IMPORTANT):

   Field 1 → System State
     0 = IDLE
     1 = ARMED

   Field 2 → Alert Status
     0 = No Motion
     1 = Motion Detected
*/
void sendToThingSpeak(int systemState, int alertStatus) {
  if (!wifiConnected) return;

  if (millis() - lastThingSpeakUpdate < thingSpeakInterval) return;

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    String url = "http://api.thingspeak.com/update?api_key=" +
                 String(thingSpeakAPIKey) +
                 "&field1=" + String(systemState) +   // Chart 1: System State
                 "&field2=" + String(alertStatus);    // Chart 2: Alert Status

    http.begin(url);
    http.GET();
    http.end();

    lastThingSpeakUpdate = millis();
  } else {
    wifiConnected = false;
    Serial.println("WiFi lost. Offline mode.");
  }
}
