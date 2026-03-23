# 💧 ESP32 IoT Water Quality Monitoring System

A real-time water quality monitoring system using ESP32 that measures key water parameters, calculates Water Quality Index (WQI) and Microplastic Risk Index (MRI), and provides a beautiful web dashboard for visualization and data management.

## 📋 Overview

This IoT-based water quality monitoring system continuously measures essential water parameters, processes the data to calculate quality indices, and presents it through an interactive web interface. The system is designed for environmental monitoring, aquaculture, and water treatment applications.

## ✨ Features

### Real-time Monitoring
- **Temperature**: DS18B20 digital temperature sensor
- **pH Level**: Analog pH sensor with calibration support
- **Electrical Conductivity (EC)**: Measures total dissolved solids
- **Turbidity**: Water clarity measurement in NTU

### Advanced Analytics
- **Water Quality Index (WQI)**: Comprehensive water quality score (0-100)
- **Microplastic Risk Index (MRI)**: Novel index combining:
  - Turbidity (40% weight)
  - EC (30% weight)
  - pH deviation (20% weight)
  - Temperature deviation (10% weight)
- **Risk Classification**: Very Low, Low, Moderate, High, Very High
- **Usability Assessment**: Drinking water suitability evaluation

### Data Management
- **SD Card Logging**: Continuous data storage in CSV format
- **Firebase Integration**: Real-time cloud synchronization
- **Sample Management**: Save and compare water samples
- **Data Export**: Download complete logs as CSV files

### Web Interface
- **Real-time Dashboard**: Live updates every 3 seconds
- **Responsive Design**: Works on desktop and mobile devices
- **Interactive Charts**: Visual comparison of saved samples
- **Advanced Settings**: View saved samples with statistical analysis

## 🛠️ Technology Stack

### Hardware
- **Microcontroller**: ESP32 (WiFi + Bluetooth)
- **Temperature**: DS18B20 digital sensor
- **pH**: Analog pH sensor (calibrated)
- **EC**: Analog conductivity sensor
- **Turbidity**: Analog turbidity sensor
- **Storage**: MicroSD card module
- **Communication**: WiFi 802.11 b/g/n

### Software
- **Framework**: Arduino/ESP32 SDK
- **Web Server**: ESP32 WebServer library
- **Database**: Firebase Realtime Database
- **Data Format**: JSON, CSV
- **Frontend**: HTML5, CSS3, JavaScript, Chart.js

## 📊 Sensor Specifications

| Parameter | Sensor | Range | Accuracy | Units |
|-----------|--------|-------|----------|-------|
| Temperature | DS18B20 | -55 to +125°C | ±0.5°C | °C |
| pH | Analog | 0-14 | ±0.1 | pH |
| EC | Analog | 0-3000 | ±5% | µS/cm |
| Turbidity | Analog | 0-3000 | ±5% | NTU |

## 🔧 Hardware Wiring

| Sensor | ESP32 Pin |
|--------|-----------|
| DS18B20 | GPIO 4 |
| Turbidity Sensor | GPIO 1 (ADC1_CH0) |
| EC Sensor | GPIO 2 (ADC1_CH1) |
| pH Sensor | GPIO 3 (ADC1_CH2) |
| SD Card CS | GPIO 10 |
| SD Card MOSI | GPIO 11 |
| SD Card SCK | GPIO 12 |
| SD Card MISO | GPIO 13 |

## 🚀 Getting Started

### Prerequisites

**Hardware Required:**
- ESP32 development board
- DS18B20 temperature sensor
- pH sensor kit (with calibration solution)
- EC sensor module
- Turbidity sensor module
- MicroSD card module
- MicroSD card (4GB+)
- Jumper wires
- Breadboard (optional)

**Software Required:**
- Arduino IDE 1.8+ or PlatformIO
- ESP32 board support package
- Required libraries (see below)

### Library Installation

Install these libraries in Arduino IDE:

```cpp
// Through Library Manager
- WiFi (built-in)
- WebServer (built-in)
- SPI (built-in)
- SD (built-in)
- OneWire by Paul Stoffregen
- DallasTemperature by Miles Burton

// Manual installation (for HTTPClient with SSL)
- HTTPClient (built-in)
- WiFiClientSecure (built-in)
