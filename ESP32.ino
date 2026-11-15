#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "DFRobot_HumanDetection.h"
#include <TimeLib.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// ---------------- Wi-Fi credentials ----------------
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// ---------------- Firebase credentials ----------------
#define API_KEY ""
#define DATABASE_URL "https:....firebaseio.com"

// ---------------- Firebase objects ----------------
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// ---------------- Patient + Paths ----------------
String patientID = "";
String basePath = "";

// ---------------- Sensor object ----------------
DFRobot_HumanDetection hu(&Serial1);

// ---------------- NTP setup ----------------
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, 16, 17);

  // ---- WiFi connect ----
  Serial.print("🔌 Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\n✅ WiFi Connected");

  // ---- NTP ----
  timeClient.begin();

  // ---- Firebase Setup ----
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  // ---- Correct login method ----
  auth.user.email = "...@gmail.com";
  auth.user.password = "finalyearproject2025";

  Serial.println("🔐 Logging into Firebase...");

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // ---- Wait until Firebase is ready ----
  unsigned long startTime = millis();
  while (!Firebase.ready()) {
    Serial.println("⏳ Waiting for Firebase...");
    delay(1500);
    if (millis() - startTime > 20000) {
      Serial.println("❌ Firebase initialization timeout.");
      return;
    }
  }
  Serial.println("✅ Firebase Ready!");

  // ---- Persistent UID ----
  patientID = auth.token.uid.c_str();
  basePath = "/patients/" + patientID;
  Serial.println("🆔 Patient ID: " + patientID);

  // ---- Upload static profile ----
  Firebase.RTDB.setString(&fbdo, basePath + "/profile/name", "John Doe");
  Firebase.RTDB.setInt(&fbdo, basePath + "/profile/age", 45);
  Firebase.RTDB.setString(&fbdo, basePath + "/profile/gender", "Male");
  Firebase.RTDB.setString(&fbdo, basePath + "/profile/condition", "Hypertension");
  Serial.println("✅ Patient profile uploaded");

  // ---- Sensor ----
  Serial.println("⚙ Initializing mmWave sensor...");
  while (hu.begin() != 0) {
    Serial.println("❌ Sensor init error! Retrying...");
    delay(1000);
  }
  Serial.println("✅ Sensor initialized!");
  hu.configLEDLight(hu.eHPLed, 1);
  hu.sensorRet();
}

// =====================================================
// LOOP
// =====================================================
void loop() {

  // ---- Time ----
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();

  char timestamp[25];
  sprintf(timestamp, "%04d-%02d-%02dT%02d:%02d:%02dZ",
          year(epochTime), month(epochTime), day(epochTime),
          hour(epochTime), minute(epochTime), second(epochTime));

  String timeRecorded = String(timestamp);

  // ---- Sensor reads ----
  int presence = hu.smHumanData(hu.eHumanPresence);
  int movement = hu.smHumanData(hu.eHumanMovement);
  int movementRange = hu.smHumanData(hu.eHumanMovingRange);
  int respiration = hu.getBreatheValue();
  int heartRate = hu.getHeartRate();

  // ---- Ensure Firebase is ready ----
  if (!Firebase.ready()) {
    Serial.println("⏳ Firebase not ready — upload skipped.");
    delay(3000);
    return;
  }

  // ---- Upload ----
  String logPath = basePath + "/logs/" + timeRecorded;

  Firebase.RTDB.setInt(&fbdo, logPath + "/presence", presence);
  Firebase.RTDB.setInt(&fbdo, logPath + "/movement", movement);
  Firebase.RTDB.setInt(&fbdo, logPath + "/movementRange", movementRange);
  Firebase.RTDB.setInt(&fbdo, logPath + "/respirationRate", respiration);
  Firebase.RTDB.setInt(&fbdo, logPath + "/heartRate", heartRate);
  Firebase.RTDB.setString(&fbdo, logPath + "/timeRecorded", timeRecorded);

  Serial.println("✅ Uploaded vitals at " + timeRecorded);

  delay(5000);
}
