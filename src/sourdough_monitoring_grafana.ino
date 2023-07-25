#include <Arduino.h>
#include "vl53l4cd_class.h"
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

SET_LOOP_TASK_STACK_SIZE(16*1024);

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
#define DEV_I2C Wire
VL53L4CD sensor_vl53l4cd_sat(&DEV_I2C, 2);

WebServer httpServer(80);

Measurement currentMeassurement {0, NAN, NAN, NAN};
QuatarlyMeasurementType quartalyMeassurement;

time_t epochInQuarters(time_t epoch) {
  return epoch / (60*(60/MEASUREMENTS_PER_HOUR));
}

void addMeasurement(Measurement& newM) {
  Measurement m = quartalyMeassurement.last();

  if(epochInQuarters(m.time) != epochInQuarters(newM.time) && isValidMeasurement(newM)) {
    Serial.println("Saving measurement with timestamp " + String(newM.time));
    quartalyMeassurement.push(newM);
    
    saveCircularBufferFromFile();
  } else {
    Serial.println("Not saving measurement");
  }
}

// Get height of starter
float getHeight() {
  float dist=0;

  #ifdef EMULATE_SENSOR
  dist=rand() % 20 - 1;
  #else
  dist=getRange();
  #endif

  if(dist == NAN)
    return NAN;

  float height_from_bottom = 16.50;
  float height = height_from_bottom - dist;

  if (height > 0) {
    return height;
  } else {
    return 0; // Distance below zero set to bottom of jar
  };
}

void reconnectWifi() {
  const u_int64_t DELAY = 20000;
  static u_int64_t previous_time = 0;
  u_int64_t current_time = millis(); // number of milliseconds since the upload

  // checking for WIFI connection
  if ((WiFi.status() != WL_CONNECTED) && (current_time - previous_time >= 20000)) {
    Serial.print(millis());
    Serial.println("Reconnecting to WIFI network");
    WiFi.disconnect();
    WiFi.reconnect();
    previous_time = current_time;
  }
}

void setupRangeSensor() {
  Serial.println("Adafruit VL53L4CD");
  // Initialize I2C bus.
  DEV_I2C.begin();

  // Configure VL53L4CD satellite component.
  sensor_vl53l4cd_sat.begin();

  // Switch off VL53L4CD satellite component.
  sensor_vl53l4cd_sat.VL53L4CD_Off();

  //Initialize VL53L4CD satellite component.
  sensor_vl53l4cd_sat.InitSensor();

  // Program the highest possible TimingBudget, without enabling the
  // low power mode. This should give the best accuracy
  sensor_vl53l4cd_sat.VL53L4CD_SetRangeTiming(200, 0);

  // Start Measurements
  sensor_vl53l4cd_sat.VL53L4CD_StartRanging();
}

float getRange() {
  uint8_t NewDataReady = 0;
  VL53L4CD_Result_t results;
  uint8_t status;

  do {
    status = sensor_vl53l4cd_sat.VL53L4CD_CheckForDataReady(&NewDataReady);
  } while (!NewDataReady);

  if ((status == 0) && (NewDataReady != 0)) {
    // (Mandatory) Clear HW interrupt to restart measurements
    sensor_vl53l4cd_sat.VL53L4CD_ClearInterrupt();

    // Read measured distance. RangeStatus = 0 means valid data
    sensor_vl53l4cd_sat.VL53L4CD_GetResult(&results);
    Serial.printf("Raw measurement %u mm\n", results.distance_mm);
    return results.distance_mm/10.0;
  }

  return NAN;
}

void setupWifi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int loopCount=0;

  Serial.println("Connecting to WiFi..");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    loopCount++;
    if(loopCount > 60) {
      Serial.print("Wifi connection timeout. Restarting esp!");
      WiFi.disconnect();
      ESP.restart();
    }
  }
  Serial.println("\nConnected to the WiFi network");
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
  doc[F("temperature")] = round2(m.temperature);
  doc[F("humidity")] = round2(m.humidity);
  doc[F("height")] = round2(m.height);

  return doc;
}

double round2(double value) {
   return (int)(value * 100 + 0.5) / 100.0;
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

String createQuartalyDoc(QuatarlyMeasurementType& quartalyMeassurement) {
  MeasurementJsonType doc;

  String buffer = "[";
  String jsonBuffer;
  bool first = true;

  for(int i=0; i<quartalyMeassurement.size(); i++) {
    auto m = quartalyMeassurement[i];
    if(isValidMeasurement(m)) {
        doc = createDoc(m);
        if (first) {
          first=false;
        } else {
          buffer += ",";
        }
        serializeJson(doc, buffer);
    }
  }

  buffer+="]";

  return buffer;
}

#define WEEKQUATERS_FILENAME "/weekquaters"

void saveCircularBufferFromFile() {
  File f = LittleFS.open(WEEKQUATERS_FILENAME, FILE_WRITE);
  if (!f) {
    Serial.printf(PSTR("Open failed for file %s\n"), WEEKQUATERS_FILENAME);
    ESP.restart();
  }
  f.write((uint8_t*)&quartalyMeassurement, sizeof(QuatarlyMeasurementType));

  f.close();
  Serial.printf(PSTR("Data stored to file: %s\n"), WEEKQUATERS_FILENAME);
}

void loadCircularBufferFromFile() {
  File f = LittleFS.open(WEEKQUATERS_FILENAME, FILE_READ);
  if (!f) {
    Serial.printf(PSTR("Open failed for file %s"), WEEKQUATERS_FILENAME);
    return;
  }
  f.read((uint8_t*)&quartalyMeassurement, sizeof(QuatarlyMeasurementType));

  f.close();
  Serial.printf(PSTR("Data read from file: %s\n"), WEEKQUATERS_FILENAME);
}

bool clearCircularBufferFile() {
  return LittleFS.remove(WEEKQUATERS_FILENAME);
}

void sendWeekStats(void) 
{
    String buffer;

    buffer = createQuartalyDoc(quartalyMeassurement);

    httpServer.send(200, F("application/json"), buffer);
}

void clearWeekStats(void) 
{
    if(clearCircularBufferFile()) {
      quartalyMeassurement.clear();
      httpServer.send(200, F("application/json"), F("{ \"message\": \"stats deleted\"}"));
    } else {
      httpServer.send(409, F("application/json"), F("{ \"message\": \"stats could not be deleted\"}"));
    }
    
}

void setupWebserver() {
  httpServer.on("/stats/week", sendWeekStats);
  httpServer.on("/stats/clear", clearWeekStats);
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
  loadCircularBufferFromFile();
  setupRangeSensor();
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

  reconnectWifi();
  
  delay(50);
}

