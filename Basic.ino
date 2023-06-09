#include <Arduino.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <ESP8266WiFi.h>

#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#define WIFI_SSID "Primo G9"
#define WIFI_PASSWORD "12345678"

#define API_KEY "AIzaSyB08XFsyLiTN8AyTmfPkD8-bpIqlz9q2I4"
#define DATABASE_URL "https://attendancetpi-default-rtdb.asia-southeast1.firebasedatabase.app/"  //<databaseName>.firebaseio.com or <databaseName>.<region>.firebasedatabase.app
#define USER_EMAIL "md.ismailhosenismailjames@gmail.com"
#define USER_PASSWORD "1234567890"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

void setup() {

  Serial.begin(9600);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  unsigned long ms = millis();
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;  // see addons/TokenHelper.h

  fbdo.setBSSLBufferSize(2048, 2048);
  fbdo.setResponseSize(2048);

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  Firebase.setDoubleDigits(5);
  config.timeout.serverResponse = 10 * 1000;
  config.timeout.wifiReconnect = 10 * 1000;

  // get and set the time for NTP Client
  timeClient.begin();
  timeClient.update();
  setTime(timeClient.getEpochTime());
}

String command = "";
int id = -1;
int roll = -1;
int atten_id = -1;
int offline[500];
int lenOffline = 0;

void loop() {
  if(lenOffline != 0 && WiFi.status() == WL_CONNECTED){
    // next
  }
  Serial.print("Enter your Comand : ");
  while (command == "") {
    if (Serial.available()) {
      command = Serial.readString();
    }
  }
  Serial.println(command);

  if (command == "reg" || command == "REG") {
    Serial.print("Enter Your id : ");
    while (id == -1) {
      if (Serial.available()) {
        id = Serial.parseInt();
      }
    }
    Serial.println(id);

    Serial.print("Enter Your roll : ");
    while (roll == -1) {
      if (Serial.available()) {
        roll = Serial.parseInt();
      }
    }
    Serial.println(roll);
    Serial.println("Registaring you in database.");
    if (Firebase.RTDB.setInt(&fbdo, ("/data/309/" + String(id)).c_str(), roll)) {
      Serial.println("Successful");
    } else {
      Serial.println(fbdo.errorReason().c_str());
    }

  } else if (command == "atten" || command == "ATTEN") {
    Serial.print("Enter Your FingerPrint ID : ");
    while (atten_id == -1) {
      if (Serial.available()) {
        atten_id = Serial.parseInt();
      }
    }
    Serial.println(atten_id);
    Serial.println("Taking attendence...");
    char timestamp[15];
    sprintf(timestamp, "%04d%02d%02d/%02d%02d%02d", year(), month(), day(), hour(), minute(), second());
    if (WiFi.status() == WL_CONNECTED) {
      if (Firebase.RTDB.getInt(&fbdo, ("/data/309/" + String(id)).c_str())) {
        int attenRoll = fbdo.to<int>();
        if (Firebase.RTDB.setInt(&fbdo, ("atten/computer/309/" + String(timestamp)).c_str(), attenRoll)) {
          Serial.println("Successful");
        } else {
          Serial.println(fbdo.errorReason().c_str());
        }
      }else {
      Serial.println(fbdo.errorReason().c_str());
      }
    }else{
      offline[lenOffline] = atten_id;
      lenOffline++;
      Serial.println("Offline!. Data stored. We will try automatically.");
    }


  } else if (command == "time") {
    Serial.println("Unknown Command.");
  }

  command = "";
  roll = -1;
  id = -1;
  atten_id = -1;
}
