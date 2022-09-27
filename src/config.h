#pragma once

#if __has_include("config_secret.h")
    #include "config_secret.h"
#endif

/*
Add and set these values in config_secret.h

#define WIFI_SSID     "ssid" // Add wifi name
#define WIFI_PASSWORD "key" // Add wifi passowrd
#define GC_PROM_USER "username"
#define GC_PROM_PASS "cloud secret"
*/

#define ID "sourdough" // Add unique name for this sensor
#define INTERVAL 5 //seconds

#define DHTPIN 32    // Which pin is DHT 11 connected to
#define DHTTYPE DHT11 // Type DHT 11

#define ULTRASONIC_PIN_TRIG 4 // Which pin is HC-SR04's trig connected txo
#define ULTRASONIC_PIN_ECHO 5 // Which pin is HC-SR04's echo connected to

#define GC_PORT 443
#define GC_PROM_URL "prometheus-prod-13-prod-us-east-0.grafana.net"
#define GC_PROM_PATH "/api/prom/push"
