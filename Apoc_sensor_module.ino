#include <DHT.h>
#include <DHT_U.h>
#include <BH1750.h>
#include <Wire.h>
#include "LowPower.h"
#include <SPI.h>
#include <LoRa.h>

#define ID "0002" //this is being uploaded to the replica module
#define DHTPIN 8
#define SOIL_DATA A0
#define SOIL_PWR 7
#define BATTERY A1

#define DHTTYPE    DHT11     // DHT 11

DHT_Unified dht(DHTPIN, DHTTYPE);
BH1750 lightMeter;

uint32_t delayMS;

int count = 0;
float temperature = 0;
float humidity = 0;
float lux = 0;
float soil = 0;
float battery = 0;

void setup() {
  //pinMode(LED_BUILTIN, OUTPUT); //this is connected to spi SCK pin, hence decided to disable to prevent any conflict
  pinMode(SOIL_PWR, OUTPUT); 
  Serial.begin(115200);
  Serial.println("Starting");

  dht.begin();
  Serial.println(F("DHT Begin"));

  Wire.begin();

  lightMeter.begin(BH1750::ONE_TIME_HIGH_RES_MODE);

  Serial.println(F("BH1750 Test begin"));

  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  delayMS = sensor.min_delay / 1000;
}

void loop() {
  count = 0;
  temperature = 0;
  humidity = 0;
  lux = 0;
  soil = 0;
  digitalWrite(SOIL_PWR, HIGH);
  //digitalWrite(LED_BUILTIN, HIGH);

  if (!LoRa.begin(433E6)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  //LoRa.setTxPower(12);
  Serial.println("LoRa wakeup");
  
  for (int i = 0; i < 5; i++) {
    delay(delayMS);
    sensors_event_t event;
    dht.temperature().getEvent(&event);
    if (isnan(event.temperature)) {
      Serial.println(F("Error reading temperature!"));
    }
    else {
      temperature += event.temperature;
      Serial.print(F("Temperature: "));
      Serial.print(temperature);
      Serial.println(F("°C"));
    }
    dht.humidity().getEvent(&event);
    if (isnan(event.relative_humidity)) {
      Serial.println(F("Error reading humidity!"));
    }
    else {
      humidity += event.relative_humidity;
      Serial.print(F("Humidity: "));
      Serial.print(humidity);
      Serial.println(F("%"));
    }

    battery += 2 * analogRead(BATTERY) * (3.3 / 1023.0);

    soil += analogRead(SOIL_DATA) * (3.3 / 1023.0);

    lux += lightMeter.readLightLevel();
    Serial.print("Light intensity : ");
    Serial.println(lux);
    lightMeter.configure(BH1750::ONE_TIME_HIGH_RES_MODE);
    count++;
  }

  temperature = temperature/count;
  humidity = humidity/count;
  lux = lux/count;
  soil = soil/count;
  battery = battery/count;

  float checksum = temperature + humidity + lux + soil;
  String message = String(ID)+","+String(battery)+","+String(checksum)+","+String(temperature)+","+String(humidity)+","+String(lux)+","+String(soil);
  
  Serial.print("message : ");
  Serial.println(message);

  LoRa.beginPacket();
  Serial.println("Packet begin");
  LoRa.print(message);
  Serial.println("print message");
  LoRa.endPacket();
  Serial.println("end packet");
  //LoRa.sleep();
  LoRa.end();
  Serial.println("LoRa sleep");

  Serial.println("going to sleep");
  Serial.flush();
  //digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(SOIL_PWR, LOW);
  for (int i = 0; i < 113; i++) { //set to sleep for 15mins
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  }
}
