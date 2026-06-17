# Smart Glove – Indian Sign Language (ISL) Gesture Translation System

## Overview

Smart Glove is a wearable embedded system that translates Indian Sign Language (ISL) gestures into spoken audio in real time. The system combines touch sensors and wrist-orientation data from an MPU6050 accelerometer to recognize gestures and provide instant voice output.

Unlike many sign-language translation systems, Smart Glove operates fully offline using onboard audio playback while also supporting Bluetooth-based customization through an Android application.

This project was developed as part of B.Tech coursework to explore embedded systems, real-time signal processing, sensor fusion, and human-computer interaction.

---

## Key Features

* Real-time ISL gesture recognition
* Fully offline operation using onboard audio playback
* Hybrid audio system:

  * DFPlayer Mini + Speaker for offline voice output
  * Bluetooth-connected Android app for custom phrases
* ESP32 dual-core architecture
* Median-filtered sensor readings for noise reduction
* Multi-sample gesture confirmation to reduce false triggers
* Confidence-based gesture scoring
* Custom gesture creation via Android app
* Persistent storage of user-defined gestures in ESP32 flash memory
* Automatic sensor health monitoring and recovery

---

## Problem Statement

Communication between hearing/speech-impaired individuals and people unfamiliar with sign language remains a challenge.

The goal of this project is to create a portable and affordable wearable device capable of translating hand gestures into understandable speech without requiring internet access or external cloud services.

---

## System Architecture

### Input Layer

* 5 Touch Sensors
* MPU6050 Accelerometer

### Processing Layer

* ESP32
* Gesture Matching Algorithm
* Sensor Fusion
* Confidence Scoring
* Median Filtering
* Multi-Sample Verification

### Output Layer

* DFPlayer Mini
* Speaker
* Android App (Bluetooth Mode)

---

## Hardware Components

| Component         | Purpose                     |
| ----------------- | --------------------------- |
| ESP32             | Main controller             |
| MPU6050           | Wrist orientation detection |
| 5 Touch Sensors   | Finger position detection   |
| DFPlayer Mini     | Audio playback              |
| MicroSD Card      | Audio storage               |
| Speaker           | Voice output                |
| Push Button       | Calibration mode            |
| Bluetooth Classic | Android communication       |

---

## Gesture Recognition Method

The system identifies gestures using two independent signals:

### 1. Finger Touch Pattern (50%)

Each touch sensor represents a finger state.

Example:

Thumb = 1
Index = 0
Middle = 1
Ring = 0
Little = 0

Touch Pattern:

1,0,1,0,0

### 2. Wrist Orientation (50%)

The MPU6050 measures wrist position using X-axis and Z-axis acceleration values.

Each gesture stores:

* Reference X value
* Reference Z value
* Allowed tolerance range

---

## Noise Reduction Techniques

To improve reliability, multiple filtering methods are implemented:

### Median Filtering

The ESP32 stores multiple accelerometer samples and calculates the median value to reduce noise.

### Confidence Scoring

Each detected gesture is assigned a confidence score based on:

* Touch pattern match
* Accelerometer match

Only gestures above the minimum confidence threshold are considered valid.

### Multi-Sample Confirmation

A gesture must appear in:

4 out of 5 consecutive samples

before activation.

This significantly reduces accidental triggers caused by temporary hand movements.

### Cooldown Mechanism

An 800 ms cooldown period prevents repeated triggering of the same gesture.

---

## Supported Gestures

Current implementation includes:

### Letters

A, B, C, D, E, F, G, H, I, J, K, L, M

### Numbers

1, 2

### Additional Phrases

Custom phrases can be added through the Android application.

---

## Hybrid Audio Playback System

### Offline Mode

Pre-recorded audio files are stored on a microSD card and played using:

* DFPlayer Mini
* Speaker

No smartphone or internet connection is required.

### Bluetooth Mode

The Android application can:

* Add new gestures
* Store custom phrases
* Receive recognized gestures
* Generate speech using Android Text-to-Speech

This allows the system to support unlimited user-defined phrases.

---

## Android App Features (Phase 2)

The companion application allows users to:

* Connect via Bluetooth Classic
* Calibrate sensor values
* Create custom gestures
* Assign custom spoken text
* Delete stored gestures
* Manage gesture library

Custom gestures are saved in ESP32 flash memory using the Preferences library.

---

## Embedded Software Features

### Dual-Core ESP32 Architecture

Core 0:

* Bluetooth communication
* Android app interaction

Core 1:

* Real-time gesture processing
* Sensor acquisition
* Recognition algorithm

This separation improves responsiveness and system stability.

### Sensor Health Monitoring

The firmware continuously monitors MPU6050 status and attempts automatic recovery if communication errors occur.

### Persistent Storage

User-created gestures remain available after power cycles using non-volatile flash memory.

---

## Results

### Testing Summary

* Total Test Trials: 290
* Gestures Evaluated: A–J
* Average Recognition Accuracy: 87.9%

### Performance Highlights

* Real-time response
* Low false-trigger rate
* Fully offline operation
* Stable Bluetooth integration
* Successful custom gesture storage and retrieval

---

## Software Stack

### Embedded

* C++
* Arduino Framework
* ESP32

### Libraries

* Adafruit_MPU6050
* Adafruit_Sensor
* DFRobotDFPlayerMini
* BluetoothSerial
* Preferences

---

## Future Improvements

* Support full ISL alphabet
* Machine-learning-based gesture classification
* Rechargeable battery integration
* Mobile app gesture training wizard
* Cloud synchronization for gesture profiles
* Multi-language speech output

