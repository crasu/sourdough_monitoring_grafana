#pragma once

#if __has_include("config_secret.h")
    #include "config_secret.h"
#endif

#define ID "sourdough" // Add unique name for this sensor
#define INTERVAL 15 //seconds
#define MEASUREMENTS_PER_HOUR 20 //normally 4

#define DHTPIN 32    // Which pin is DHT 11 connected to
#define DHTTYPE DHT11 // Type DHT 11

#define ULTRASONIC_PIN_TRIG 4 // Which pin is HC-SR04's trig connected txo
#define ULTRASONIC_PIN_ECHO 5 // Which pin is HC-SR04's echo connected to

#define EMULATE_SENSOR 1