#include <Arduino.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <Adafruit_Fingerprint.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#define WIFI_SSID "Jahid"
#define WIFI_PASSWORD "Jahid1122"

// for fingerprint
#if (defined(__AVR__) || defined(ESP8266)) && !defined(__AVR_ATmega2560__)
#define fingerRX 14
#define fingerTX 12
SoftwareSerial mySerial(fingerRX, fingerTX);
#else
#define mySerial Serial1
#endif
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
uint8_t id;

// for firebase
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
  //for fingerprint
  while (!Serial)
    ;
  delay(100);
  finger.begin(57600);
  delay(5);
  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
  } else {
    Serial.println("Did not find fingerprint sensor :(");
    while (1) { delay(1); }
  }

  // Serial.println(F("Reading sensor parameters"));
  // finger.getParameters();
  // Serial.print(F("Status: 0x"));
  // Serial.println(finger.status_reg, HEX);
  // Serial.print(F("Sys ID: 0x"));
  // Serial.println(finger.system_id, HEX);
  // Serial.print(F("Capacity: "));
  // Serial.println(finger.capacity);
  // Serial.print(F("Security level: "));
  // Serial.println(finger.security_level);
  // Serial.print(F("Device address: "));
  // Serial.println(finger.device_addr, HEX);
  // Serial.print(F("Packet len: "));
  // Serial.println(finger.packet_len);
  // Serial.print(F("Baud rate: "));
  // Serial.println(finger.baud_rate);


  // for firebase
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

String room = "311";
String command = "";
int roll = -1;
int atten_id = -1;
int offline[500];
int lenOffline = 0;

void loop() {
  if (year() < 2023) {
    Serial.println("Time is not currect..");
    timeClient.update();
    setTime(timeClient.getEpochTime());
    delay(500);
    return;
  }
  if (lenOffline != 0 && WiFi.status() == WL_CONNECTED) {
    char timestamp[15];
    sprintf(timestamp, "%04d%02d%02d/%02d%02d%02d", year(), month(), day(), hour(), minute(), second());
    for (int i = lenOffline; i >= 0; i--) {
      if (Firebase.RTDB.getInt(&fbdo, ("data/" + room + "/" + String(offline[i])).c_str())) {
        int attenRoll = fbdo.to<int>();
        if (Firebase.RTDB.setInt(&fbdo, ("atten/computer/" + room + "/" + String(timestamp)).c_str(), attenRoll)) {
          Serial.println("Successful");
          offline[lenOffline--] = 0;
          lenOffline--;
        } else {
          break;
          Serial.println(fbdo.errorReason().c_str());
        }
      } else {
        Serial.println(fbdo.errorReason().c_str());
        break;
      }
    }
  }
  Serial.print("Enter your command : ");

  if (Firebase.RTDB.getBool(&fbdo, "/com/enroll/311/rs")) {
    bool rs = fbdo.to<bool>();
    if (rs == false) {
      command = "REG";
    } else {
      command = "ATTEN";
    }
  }

  if (command == "reg" || command == "REG") {
    Serial.print("Enter Your roll : ");
    // while (roll == -1) {
    //   if (Serial.available()) {
    //     roll = Serial.parseInt();
    //   }
    // }
    fbdo.clear();
    if (Firebase.RTDB.getInt(&fbdo, "/com/enroll/311/roll")) {
      roll = fbdo.to<int>();
      Serial.println(roll);
      Serial.println("The roll is : ");
    } else {
      Serial.println(fbdo.errorReason());
      return;
    }
    if (Firebase.RTDB.getInt(&fbdo, "/com/enroll/311/time")) {
      int t = (minute() * 60 + second()) - fbdo.to<int>();
      Serial.print("The time is : ");
      Serial.println(t);

      if (t < 0) {
        t = t * -1;
      }
      if (t > 60) {
        Serial.println("Time Over");
        if (Firebase.RTDB.setBool(&fbdo, "/com/enroll/311/rs", true)) {
          return;
        }
      }
    } else {
      Serial.println(fbdo.errorReason());
      return;
    }

    if (Firebase.RTDB.getInt(&fbdo, ("data/" + room + "/" + "finger").c_str())) {
      id = fbdo.to<int>();
      id++;
      if (getFingerprintEnroll()) {
        if (Firebase.RTDB.setInt(&fbdo, ("data/" + room + "/" + String(id)).c_str(), roll)) {
          if (Firebase.RTDB.setInt(&fbdo, ("data/" + room + "/" + "finger").c_str(), id)) {
            if (Firebase.RTDB.setBool(&fbdo, "/com/enroll/311/rs", true)) {
              Serial.println("Successfully Enrolled");
            }
          } else {
            Serial.println(fbdo.errorReason());
          }
        } else {
          Serial.println(fbdo.errorReason());
        }
      } else {
        Serial.println("Try Again");
      }

    } else {
      Serial.println(fbdo.errorReason());
    }

    Serial.println("Registaring you in database.");
    if (Firebase.RTDB.setInt(&fbdo, ("/data/" + room + "/" + String(id)).c_str(), roll)) {
      Serial.println("Successful");
    } else {
      Serial.println(fbdo.errorReason().c_str());
    }

  } else if (command == "atten" || command == "ATTEN") {
    Serial.print("Scan your finger : ");
    while (true) {
      atten_id = getFingerprintID();
      if (atten_id == -1) {
        Serial.println("Try again...");
      } else if (atten_id == -2) {
        Serial.println("Did not found mach.");
        return;
      } else {
        break;
      }
    }


    Serial.println(atten_id);
    Serial.println("Taking attendence...");
    char timestamp[15];
    sprintf(timestamp, "%04d%02d%02d/%02d%02d%02d", year(), month(), day(), hour(), minute(), second());
    if (WiFi.status() == WL_CONNECTED) {
      if (Firebase.RTDB.getInt(&fbdo, ("data/" + room + "/" + String(atten_id)).c_str())) {
        int attenRoll = fbdo.to<int>();
        if (Firebase.RTDB.setInt(&fbdo, ("atten/computer/" + room + "/" + String(timestamp)).c_str(), attenRoll)) {
          Serial.println("Successful");
        } else {
          offline[lenOffline] = atten_id;
          lenOffline++;
          Serial.println("Offline!. Data stored. We will try automatically.");
          Serial.println(fbdo.errorReason().c_str());
        }
      } else {
        offline[lenOffline] = atten_id;
        lenOffline++;
        Serial.println("Offline!. Data stored. We will try automatically.");
        Serial.println(fbdo.errorReason().c_str());
      }
    } else {
      offline[lenOffline] = atten_id;
      lenOffline++;
      Serial.println("Offline!. Data stored. We will try automatically.");
    }


  } else if (command == "time") {
    Serial.println("Unknown Command.");
  }

  command = "";
  roll = -1;
  atten_id = -1;
}


// fingerprint enrolment
bool getFingerprintEnroll() {

  int p = -1;
  Serial.print("Waiting for valid finger to enroll as #");
  Serial.println(id);
  int x = minute() * 60 + second();
  int d = 0;
  while (p != FINGERPRINT_OK) {
    d = (minute() * 60 + second()) - x;
    if (d < 0) {
      d = d * -1;
    }
    if (d > 60) {
      return false;
      ;
    }
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.println(".");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        break;
      default:
        Serial.println("Unknown error");
        break;
    }
  }

  // OK success!

  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return false;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return false;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return false;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return false;
    default:
      Serial.println("Unknown error");
      return false;
  }

  Serial.println("Remove finger");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  Serial.print("ID ");
  Serial.println(id);
  p = -1;
  Serial.println("Place same finger again");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.print(".");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        break;
      default:
        Serial.println("Unknown error");
        break;
    }
  }

  // OK success!

  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return false;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return false;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return false;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return false;
    default:
      Serial.println("Unknown error");
      return false;
  }

  // OK converted!
  Serial.print("Creating model for #");
  Serial.println(id);

  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    Serial.println("Prints matched!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return false;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    Serial.println("Fingerprints did not match");
    return false;
  } else {
    Serial.println("Unknown error");
    return false;
  }

  Serial.print("ID ");
  Serial.println(id);
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK) {
    Serial.println("Stored!");
    return true;
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return false;
  } else if (p == FINGERPRINT_BADLOCATION) {
    Serial.println("Could not store in that location");
    return false;
  } else if (p == FINGERPRINT_FLASHERR) {
    Serial.println("Error writing to flash");
    return false;
  } else {
    Serial.println("Unknown error");
    return false;
  }

  return true;
}


int getFingerprintID() {
  uint8_t p = finger.getImage();
  while (p == FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return -1;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      return -1;
    default:
      Serial.println("Unknown error");
      return -1;
  }

  // OK success!

  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return -1;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return -1;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return -1;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return -1;
    default:
      Serial.println("Unknown error");
      return -1;
  }

  // OK converted!
  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    Serial.println("Found a print match!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return -1;
  } else if (p == FINGERPRINT_NOTFOUND) {
    Serial.println("Did not find a match");
    return -2;
  } else {
    Serial.println("Unknown error");
    return -1;
  }

  // found a match!
  Serial.print("Found ID #");
  Serial.print(finger.fingerID);
  Serial.print(" with confidence of ");
  Serial.println(finger.confidence);

  return finger.fingerID;
}

// returns -1 if failed, otherwise returns ID #
int getFingerprintIDez() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return -1;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return -1;

  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK) return -1;

  // found a match!
  Serial.print("Found ID #");
  Serial.print(finger.fingerID);
  Serial.print(" with confidence of ");
  Serial.println(finger.confidence);
  return finger.fingerID;
}
