#include <Arduino.h>
#include <HCSR04.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <time.h>
#include <LittleFS.h>
#include <CircularBuffer.h>

#include "certificates.h"
#include "config.h"
#include "Index.h"

int loopCounter = 0;

struct Measurement {
  time_t time;
  float temperature;
  float humidity;
  float height;
};

typedef StaticJsonDocument<1024> MeasurementJsonType;
typedef CircularBuffer<Measurement, MEASUREMENTS_PER_HOUR*24*7> QuatarlyMeasurementType;

// DHT Sensor
//DHT dht(DHTPIN, DHTTYPE);

// Ultrasonic Sensor
UltraSonicDistanceSensor distanceSensor(ULTRASONIC_PIN_TRIG, ULTRASONIC_PIN_ECHO);  

WebServer httpServer(80);

Measurement currentMeassurement {0, NAN, NAN, NAN};
QuatarlyMeasurementType quartalyMeassurement;

time_t epochInQuarters(time_t epoch) {
  return epoch / (60*(60/MEASUREMENTS_PER_HOUR));
}

void addMeasurement(Measurement& newM) {
  Measurement m = quartalyMeassurement.last();

  if(epochInQuarters(m.time) != epochInQuarters(newM.time)) {
    Serial.println("Saving measurement with timestamp " + String(newM.time));
    quartalyMeassurement.push(newM);
  } else {
    Serial.println("Not saving measurement");
  }
}

// Get height of starter
float getHeight() {
  float dist=NAN;
  float ret=0;

  for(int retry=0; retry <= 3; retry++) {
    #ifdef EMULATE_SENSOR
    dist=rand() % 20;
    #else
    dist=distanceSensor.measureDistanceCm()
    #endif

    float height_from_bottom = 16.50;
    float height = height_from_bottom - dist;

    if (height == -1) {
      ret=NAN;
    }

    if (height > 0) {
      ret=height;
      return height;
    };

    ret=0;
  }

  return ret;
}

void setupWifi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
 
  Serial.println("Connected to the WiFi network");
}

void sendMainPage(void)
{
    httpServer.send(200, "text/html", MAIN_PAGE);
}

bool isValidMeasurement(Measurement& m) {
  return not(m.time == 0 || isnan(m.temperature || isnan(m.humidity) || isnan(m.height)));
}

MeasurementJsonType createDoc(Measurement& m) {
  MeasurementJsonType doc;

  doc[F("time")] = m.time;
  doc[F("temperature")] = m.temperature;
  doc[F("humidity")] = m.humidity;
  doc[F("height")] = m.height;

  return doc;
}

void sendCurrentStats(void)
{
    MeasurementJsonType doc;
    String buffer;

    if(!isValidMeasurement(currentMeassurement)) {
      httpServer.send(503, F("application/json"), F("{}"));
    }

    doc = createDoc(currentMeassurement);

    serializeJson(doc, buffer); 
    httpServer.send(200, F("application/json"), buffer);
}

MeasurementJsonType createQuartalyDoc(QuatarlyMeasurementType& quartalyMeassurement) {
  MeasurementJsonType doc;

  for(int i=0; i<quartalyMeassurement.size(); i++) {
    auto m = quartalyMeassurement[i];
    doc.add(createDoc(m));
  }
  return doc;
}

void sendWeekStats(void) 
{
    MeasurementJsonType doc;
    String buffer;

    if(!isValidMeasurement(currentMeassurement)) {
      httpServer.send(503, F("application/json"), F("{}"));
    }

    doc = createQuartalyDoc(quartalyMeassurement);

    serializeJson(doc, buffer); 
    httpServer.send(200, F("application/json"), buffer);
}

void setupWebserver() {
  httpServer.on("/stats/week", sendWeekStats);
  //httpServer.on("/stats/day", sendYearStats);
  httpServer.on("/stats/current", sendCurrentStats);
  httpServer.on("/", sendMainPage);
  httpServer.begin();
}

void setup() {
  Serial.begin(115200);
  setupWifi();
  configTime(0, 0, "pool.ntp.org");
  MDNS.begin(ID);
  LittleFS.begin(true);
  setupWebserver();
  //dht.begin();
}

time_t getTime() {
  time_t t;
  time(&t);

  return t;
}

void loop() {
  time_t time=getTime();
  static time_t lastUpdate=0;

  if((time-lastUpdate)>=INTERVAL) {
    lastUpdate=time;

    /*float hum = dht.readHumidity();
    float cels = dht.readTemperature();*/
    float height = getHeight();

    // Check if any reads failed and exit early (to try again).
    if (isnan(height) ) {
      Serial.println(F("Failed to read from sensor!"));
    } else {
      currentMeassurement = Measurement{getTime(), 20, 0.4, height};
      addMeasurement(currentMeassurement);

      Serial.println("Measured " + String(height) + " cm");
    }
  }

  httpServer.handleClient();
  
  delay(50);
}

