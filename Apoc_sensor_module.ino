#include <DHT.h>
#include <DHT_U.h>
#include <BH1750.h>
#include <Wire.h>
#include "LowPower.h"
#include <SPI.h>
#include <LoRa.h>

#define DEBUG 1
#define ID "0002"
#define DHTPIN 8
#define SOIL_DATA A0
#define SOIL_PWR 7
#define BATTERY A1
#define DHTTYPE DHT11
#define SAMPLE_COUNT 5
#define SLEEP_CYCLES 113
#define VREF 3.3
#define LORA_FREQUENCY 433E6

DHT_Unified dht(DHTPIN, DHTTYPE);
BH1750 lightMeter;
uint32_t delayMS;

float temperature = 0;
float humidity = 0;
float lux = 0;
float soil = 0;
float battery = 0;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(SOIL_PWR, OUTPUT);
  
  #if DEBUG
  Serial.begin(115200);
  Serial.println("Starting");
  #endif

  while (!initializeSensors()) {
    indicateError();
    delay(1000);  // Wait 1 seconds before retrying
  }

  while (!initLoRa()) {
    indicateError();
    delay(1000);  // Wait 1 seconds before retrying
  }

  // If we've reached here, everything is initialized properly
  digitalWrite(LED_BUILTIN, LOW);  // Turn off LED to indicate successful initialization
}

bool initializeSensors() {
  #if DEBUG
  Serial.println("Initializing sensors...");
  #endif

  // Initialize DHT sensor
  dht.begin();
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  delayMS = sensor.min_delay / 1000;

  // Check if DHT sensor is responding
  sensors_event_t event;
  dht.temperature().getEvent(&event);
  if (isnan(event.temperature)) {
    #if DEBUG
    Serial.println("Failed to initialize DHT sensor!");
    #endif
    return false;
  }

  // Initialize I2C and BH1750 light sensor
  Wire.begin();
  if (!lightMeter.begin(BH1750::ONE_TIME_HIGH_RES_MODE)) {
    #if DEBUG
    Serial.println("Failed to initialize BH1750 sensor!");
    #endif
    return false;
  }

  #if DEBUG
  Serial.println("All sensors initialized successfully");
  #endif
  return true;
}

bool initLoRa() {
  #if DEBUG
  Serial.println("Initializing LoRa...");
  #endif

  if (!LoRa.begin(LORA_FREQUENCY)) {
    #if DEBUG
    Serial.println("LoRa initialization failed!");
    #endif
    return false;
  }

  // LoRa.setTxPower(12);
  #if DEBUG
  Serial.println("LoRa initialized successfully");
  #endif
  return true;
}

void indicateError() {
  for (int i = 0; i < 5; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
  }
}

void loop() {
  wakeupAndInitialize();
  readSensors();
  transmitData();
  sleep();
}

void wakeupAndInitialize() {
  digitalWrite(SOIL_PWR, HIGH);
  if (!LoRa.begin(LORA_FREQUENCY)) {
    #if DEBUG
    Serial.println("LoRa wakeup failed!");
    #endif
    return;
  }
  #if DEBUG
  Serial.println("LoRa wakeup successful");
  #endif
}

void readSensors() {
  temperature = 0;
  humidity = 0;
  lux = 0;
  soil = 0;
  battery = 0;

  for (int i = 0; i < SAMPLE_COUNT; i++) {
    delay(delayMS);
    readDHTSensor();
    readBatterySensor();
    readSoilSensor();
    readLightSensor();
  }

  temperature /= SAMPLE_COUNT;
  humidity /= SAMPLE_COUNT;
  lux /= SAMPLE_COUNT;
  soil /= SAMPLE_COUNT;
  battery /= SAMPLE_COUNT;

  #if DEBUG
  printSensorReadings();
  #endif
}

void readDHTSensor() {
  sensors_event_t event;
  dht.temperature().getEvent(&event);
  if (!isnan(event.temperature)) {
    temperature += event.temperature;
  }
  
  dht.humidity().getEvent(&event);
  if (!isnan(event.relative_humidity)) {
    humidity += event.relative_humidity;
  }
}

void readBatterySensor() {
  battery += 2 * analogRead(BATTERY) * (VREF / 1023.0);
}

void readSoilSensor() {
  soil += analogRead(SOIL_DATA) * (VREF / 1023.0);
}

void readLightSensor() {
  lux += lightMeter.readLightLevel();
  lightMeter.configure(BH1750::ONE_TIME_HIGH_RES_MODE);
}

void printSensorReadings() {
  Serial.println("Sensor Readings:");
  Serial.print("Temperature: "); Serial.print(temperature); Serial.println("°C");
  Serial.print("Humidity: "); Serial.print(humidity); Serial.println("%");
  Serial.print("Light intensity: "); Serial.print(lux); Serial.println(" lx");
  Serial.print("Soil moisture: "); Serial.print(soil); Serial.println(" V");
  Serial.print("Battery voltage: "); Serial.print(battery); Serial.println(" V");
}

void transmitData() {
  String message = formatMessage();
  #if DEBUG
  Serial.println("Transmitting: " + message);
  #endif
  
  for (int attempt = 0; attempt < 3; attempt++) {
    if (sendLoRaMessage(message)) {
      #if DEBUG
      Serial.println("Transmission successful");
      #endif
      break;
    }
    #if DEBUG
    Serial.println("Transmission failed. Retrying...");
    #endif
    delay(1000);
  }
}

String formatMessage() {
  float checksum = temperature + humidity + lux + soil;
  return String(ID) + "," + String(battery, 2) + "," + String(checksum, 2) + "," +
         String(temperature, 2) + "," + String(humidity, 2) + "," + 
         String(lux, 2) + "," + String(soil, 2);
}

bool sendLoRaMessage(String message) {
  LoRa.beginPacket();
  LoRa.print(message);
  return LoRa.endPacket();
}

void sleep() {
  LoRa.end();
  digitalWrite(SOIL_PWR, LOW);
  digitalWrite(LED_BUILTIN, LOW);  // Ensure LED is off during sleep
  #if DEBUG
  Serial.println("Entering sleep mode");
  Serial.flush();
  #endif
  for (int i = 0; i < SLEEP_CYCLES; i++) {
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  }
}