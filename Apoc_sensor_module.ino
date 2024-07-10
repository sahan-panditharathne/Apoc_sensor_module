#include <DHT.h>
#include <DHT_U.h>
#include <BH1750.h>
#include <Wire.h>
#include "LowPower.h"
#include <SPI.h>
#include <LoRa.h>
#include <EEPROM.h>

#define NETWORK_ID "ALPHA1" // Network identifier for the sensor group - MUST BE CHANGED BY USER

#define DEBUG 1 // Enable debug mode for serial output (1 for enabled, 0 for disabled)
#define DHTPIN 8 // Digital pin connected to the DHT sensor
#define SOIL_DATA A0 // Analog pin for soil moisture sensor data
#define SOIL_PWR 7 // Digital pin to control power to the soil moisture sensor
#define BATTERY A1 // Analog pin for battery voltage measurement
#define DHTTYPE DHT11 // Type of DHT sensor in use
#define SAMPLE_COUNT 3 // Number of samples to take for each sensor reading
#define SLEEP_CYCLES 113 // Number of 8-second sleep cycles (113 * 8 seconds ≈ 15 minutes)
#define VREF 3.3 // Reference voltage for analog readings
#define LORA_FREQUENCY 433E6 // LoRa radio frequency in Hz (433 MHz in this case)
#define EEPROM_ADDR 0 // EEPROM address to store the unique ID
#define TRANSMIT_ATTEMPT 3 //Number of attempts to transmit the message in case of failure

DHT_Unified dht(DHTPIN, DHTTYPE);
BH1750 lightMeter;
uint32_t delayMS;

float temperature = 0;
float humidity = 0;
float lux = 0;
float soil = 0;
float battery = 0;

String uniqueID;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(SOIL_PWR, OUTPUT);

  #if DEBUG
  Serial.begin(115200);
  Serial.println("Starting");
  #endif

  while (!initializeSensors()) {
    indicateError();
    delay(1000);  // Wait 1 second before retrying
  }

  while (!initLoRa()) {
    indicateError();
    delay(1000);  // Wait 1 second before retrying
  }

  if (!checkAndGenerateID()) {
    indicateError();
    while (true);  // Stop execution if ID generation failed
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
  
  for (int attempt = 0; attempt < TRANSMIT_ATTEMPT; attempt++) {
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
  static uint32_t sequence = 0;
  char buffer[150];  // Buffer for the main message parts

  char temperatureStr[10];
  char humidityStr[10];
  char luxStr[10];
  char soilStr[10];
  char batteryStr[10];

  // Convert floats to strings
  dtostrf(temperature, sizeof(temperature), 2, temperatureStr);
  dtostrf(humidity, sizeof(humidity), 2, humidityStr);
  dtostrf(lux, sizeof(lux), 2, luxStr);
  dtostrf(soil, sizeof(soil), 2, soilStr);
  dtostrf(battery, sizeof(battery), 2, batteryStr);
  
  // Format each data cluster
  char nodeInfo[50];
  snprintf(nodeInfo, sizeof(nodeInfo), "%s:%lu:%lu", uniqueID.c_str(), sequence++, millis());
  
  char envData[20];
  snprintf(envData, sizeof(envData), "%s;%s;%s;%s", temperatureStr, humidityStr, luxStr, soilStr);
 
  char powerData[20];
  snprintf(powerData, sizeof(powerData), "%s", batteryStr);
  
  // Combine all parts into the final message
  snprintf(buffer, sizeof(buffer), 
           "%s@ENVSENSOR@%s@%s@%s@",
           NETWORK_ID, nodeInfo, envData, powerData);
  
  String message = "##" + String(buffer);
  uint16_t checksumValue = calculateChecksum(message.c_str(), message.length());
  
  message += String(checksumValue, HEX);
  message += "$$";
  
  return message;
}

uint16_t calculateChecksum(const char* data, size_t length) {
  uint16_t checksum = 0;
  for (size_t i = 0; i < length; i++) {
    checksum += (uint8_t)data[i];
  }
  return checksum;
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

bool checkAndGenerateID() {
  char id[6];  // Buffer to hold the ID

  // Read ID from EEPROM
  for (int i = 0; i < 5; i++) {
    id[i] = EEPROM.read(EEPROM_ADDR + i);
  }
  id[5] = '\0';  // Null-terminate the string

  // Check if the ID is valid
  if (isValidID(id)) {
    uniqueID = String(id);
    #if DEBUG
    Serial.println("Existing ID found: " + uniqueID);
    #endif
    return true;
  }

  // Generate a new unique ID
  for (int i = 0; i < 5; i++) {
    id[i] = random(0, 10) + '0';
  }
  id[5] = '\0';  // Null-terminate the string

  // Save the new ID to EEPROM
  for (int i = 0; i < 5; i++) {
    EEPROM.write(EEPROM_ADDR + i, id[i]);
  }

  uniqueID = String(id);
  #if DEBUG
  Serial.println("New ID generated: " + uniqueID);
  #endif

  return true;
}

bool isValidID(const char* id) {
  // Check if the ID consists of 5 digits
  for (int i = 0; i < 5; i++) {
    if (!isdigit(id[i])) {
      return false;
    }
  }
  return true;
}
