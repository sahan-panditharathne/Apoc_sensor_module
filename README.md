# Apocalypes Sensor Module

The Apocalypse Sensor Kit is meant for gardening. It's a cheap, DIY suite of hardware and software that you can put together yourself, plant in the ground, and get readings for light intensity, soil moisture, humidity, and temperature. 
Read the [full documentation](https://github.com/team-watchdog/apocalypse-sensor-kit).

The Apocalypse Sensor Kit consists of three main components:
1. Sensor Modules: Battery-powered devices that collect environmental data.
2. Receiver Device: A central hub that receives and stores data from sensor modules.
2. Mobile Application: For visualizing and analyzing the collected data.

This document is a walk through of the firmware development for the Apocalypse sensor module.

## Table of Contents
1. [Hardware Requirements](#hardware-requirements)
2. [Software Dependencies](#software-dependencies)
3. [Pin Configuration](#pin-configuration)
4. [Functionality](#functionality)
5. [Message Format](#message-format)
6. [Setup and Installation](#setup-and-installation)
7. [Usage](#usage)
8. [Power Management](#power-management)
9. [Troubleshooting](#troubleshooting)
10. [Network ID Configuration](#network-id-configuration)
11. [Unique ID Generation](#unique-id-generation)

## Hardware Requirements

- Arduino board (Arduino Pro Mini 3.3V 8MHz recommended)
- DHT11 temperature and humidity sensor
- BH1750 light sensor
- Soil moisture sensor
- LoRa module (e.g., Ra-02)
- Battery for power supply

## Software Dependencies

This project requires the following libraries:

- DHT sensor library for Arduino
- Adafruit Unified Sensor
- BH1750
- LowPower
- LoRa
- Wire (for I2C communication)
- SPI
- EEPROM

Ensure these are installed in your Arduino IDE before compiling the code.

## Pin Configuration

- DHT11 sensor: Pin 8
- Soil moisture sensor data: Pin A0
- Soil moisture sensor power: Pin 7
- Battery voltage measurement: Pin A1
- BH1750 sensor: I2C pins (SDA and SCL)
- LoRa module: SPI pins

## Functionality

1. The device initializes all sensors and the LoRa module.
2. It generates or retrieves a unique ID for the node.
3. It then enters a loop of:
   - Waking up and reinitializing LoRa
   - Reading sensor data (with multiple samples for accuracy)
   - Transmitting data via LoRa
   - Entering a low-power sleep mode

## Message Format

The LoRa message is formatted as follows:
##NETWORK_ID@ENV@NODE_ID:SEQUENCE:TIMESTAMP@TEMP;HUMIDITY;LIGHT;SOIL@BATTERY@CHECKSUM$$

- `##`: Start delimiter
- `NETWORK_ID`: Identifier for the network (e.g., "WD")
- `ENV`: Message type identifier
- `NODE_ID`: Unique identifier for this sensor node
- `SEQUENCE`: Message sequence number
- `TIMESTAMP`: Current timestamp (milliseconds since boot)
- `TEMP`: Temperature reading (°C)
- `HUMIDITY`: Humidity reading (%)
- `LIGHT`: Light intensity (lux)
- `SOIL`: Soil moisture reading (%)
- `BATTERY`: Battery voltage (V)
- `CHECKSUM`: Hexadecimal checksum of the message
- `$$`: End delimiter

Example:
##WD@ENV@12345:10:5000@24.50;53.20;216.33;0.89@3.70@a08$$

## Setup and Installation

1. Connect all sensors and the LoRa module to your Arduino board according to the pin configuration.
2. Install all required libraries in your Arduino IDE.
3. Copy the provided code into your Arduino IDE.
4. Adjust the `NETWORK_ID` and `LORA_FREQUENCY` as needed.
5. Upload the code to your Arduino board.

## Usage

Once set up and powered on, the device will automatically begin its sensing and transmission cycle. Use a LoRa receiver to capture and decode the transmitted messages.

## Power Management

The device uses low-power sleep modes to conserve energy. It sleeps for approximately 15 minutes between readings (adjustable via `SLEEP_CYCLES`).

## Troubleshooting

- If sensors fail to initialize, the built-in LED will blink rapidly.
- Enable `DEBUG` mode for verbose serial output to diagnose issues.
- Ensure all connections are secure and power supply is adequate.
- Verify that the LoRa frequency matches your regional regulations.

## Network ID Configuration

The Network ID is a crucial parameter that MUST be customized for your specific implementation. It serves as a filter for the receiver device to process only messages from sensors within its group.

- Change the `NETWORK_ID` in the code to a unique, short identifier for your sensor network.
- Keep the Network ID as short as possible (e.g., "WD", "A1") to minimize transmission length.
- Ensure all sensors in your network and the corresponding receiver use the same Network ID.
- 

## Soil moisture sensor calibration

**Identify Calibration Points:**

- `dryValue`: This represents the analog value output by the sensor when the soil is completely dry. The default dry value is set to 690.
- `wetValue`: This represents the analog value output when the soil is completely saturated with water. The default wet value is set to 380.

**Collect Calibration Data:**

- Place the soil moisture sensor in a sample of completely dry soil (or leave it exposed to air) and record the analog value. This is the dry value.
- Submerge the sensor in a sample of water-saturated soil and record the analog value. This is the wet value.

**Adjusting Calibration Values:**

The default calibration values (`dryValue = 690` and `wetValue = 380`) may need to be adjusted depending on,
- Sensor Variability: Different sensors might have slight variations in their readings.
- Environmental Conditions: Soil type, salinity, and temperature can affect the sensor's readings.

## Unique ID Generation

The device generates a unique 5-digit ID for each node, which is stored in EEPROM:

- On first run, it generates a random ID based on sensor readings and analog noise.
- The ID is stored in EEPROM and reused on subsequent boots.
- This ensures each node has a persistent, unique identifier.
