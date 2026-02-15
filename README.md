# WaveControlEsp32
> Gesture-controlled IoT system based on ESP32-S3, TinyML and MQTT

---

## 📌 Overview

WaveControlEsp32 is an embedded system enabling **contactless gesture-based control of IoT devices**.

The system uses a wearable ESP32 module with motion sensors to recognize gestures and control:
- Smart home devices (lights, plugs)
- Infrared devices (TV, AC)
- Connected systems via MQTT (Home Assistant)

This project is designed for:
- Smart home automation
- Assistive technologies
- Embedded AI / TinyML applications

📄 Full project specification:  
`docs/specs/cahier_des_charges.pdf`

---

## 🧠 System Architecture

The system is composed of three main components:

### 1. Gesture Wristband
- ESP32-S3 (Seeed Studio XIAO)
- IMU sensor (accelerometer / gyroscope)
- Gesture recognition (TinyML or rule-based)

### 2. Base Station
- ESP32 or Raspberry Pi
- MQTT broker / Wi-Fi access point
- IR blaster for legacy devices

### 3. IoT Ecosystem
- Home Assistant integration
- Smart devices (Wi-Fi / Matter)
- Infrared devices (TV, air conditioner)

### Communication
- Wi-Fi
- MQTT
- IR

---

## ⚙️ Features

- Gesture recognition (TinyML / thresholds)
- MQTT communication
- Home Assistant integration
- Infrared control
- OTA firmware updates
- Battery monitoring
- Power management
- Mobile app support

---

## 🧩 Project Structure


## 🔌 Hardware

- Seeed Studio XIAO ESP32-S3  
- IMU sensor (MPU6050 or similar)  
- IR Blaster module  
- Battery module  

---

## 🚀 Build and Flash

### Requirements

- PlatformIO  
- ESP-IDF  

### Commands

```bash
pio run
pio run -t upload
pio device monitor

## 📡 Communication (MQTT)

### Example topics

wavecontrol/gesture
wavecontrol/device/control
wavecontrol/status


---

## 🤖 Gesture Recognition

### Approaches
1. Rule-based detection  
2. TinyML (TensorFlow Lite Micro)  

### Example gestures
- Shake → Toggle light  
- Rotation → Adjust brightness  
- Swipe → Change device  

---

## 🔄 OTA Updates

Firmware can be updated **over-the-air via Wi-Fi**.

---

## 🔋 Power Management

- Battery monitoring  
- Low-power modes  

---

## 🧪 Testing

Run tests:
```bash
pio test
