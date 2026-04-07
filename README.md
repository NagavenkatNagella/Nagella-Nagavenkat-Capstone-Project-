
# 🌿 Sustainable Smart Vertical Axis Wind Turbine System for Highways with IoT-Based Monitoring and Energy Management



## 📌 Overview
An advanced **IoT-based smart energy system** designed for **Vertical Axis Wind Turbines (VAWT)** integrated with **wireless power transfer for Electric Vehicles (EVs)**.

This system not only monitors environmental conditions and turbine safety but also **generates electrical energy and transmits it wirelessly to EVs using inductive coupling**.

The project combines:
- Renewable energy generation (VAWT)
- IoT monitoring (ESP8266)
- Wireless EV charging (BD139 transistor + coils)
- Cloud logging (Google Sheets)
- Real-time alert system (Email + Web Dashboard)

---

## 🚀 Key Features

### 🔋 Energy Generation
- Wind energy captured using VAWT
- Converted into electrical energy

### ⚡ Wireless EV Charging
- Energy transferred wirelessly using **inductive coupling**
- Transmitter coils embedded in road
- Receiver coils placed under EV

### 📡 IoT Monitoring
- Real-time temperature & humidity monitoring (DHT11)
- Live dashboard using ESP8266 web server

### ⚠️ Safety & Threat Detection
- Touch sensors detect tampering or intrusion
- Instant alert system

### ☁️ Cloud Integration
- Google Sheets logging every 15 seconds
- Remote monitoring capability

### 📧 Alert System
- Email notifications using SMTP (Gmail)
- Triggered during abnormal conditions

---

## 🧠 System Architecture

```
          WIND ENERGY
               ↓
            VAWT
               ↓
        Electrical Output
               ↓
        ┌───────────────┐
        │ ESP8266 Unit  │
        └───────────────┘
           ↓        ↓
   Sensors Data   Control Signals
           ↓
 ┌─────────────────────────────┐
 │ Wireless Power Transmission │
 └─────────────────────────────┘
     ↓                  ↓
Transmitter Coil   Receiver Coil (EV)
     ↓                  ↓
 Magnetic Field     Induced Voltage
           ↓
       EV Charging
```

---

## ⚡ Wireless Power Transfer (Core Concept)

The wireless energy transfer is based on **Electromagnetic Induction**.

### 📖 Working Principle

1. The **BD139 transistor** acts as a **switching amplifier**
2. It generates **high-frequency oscillations**
3. Current flows through the **transmitter coil**
4. This creates a **changing magnetic field**
5. The **receiver coil (in EV)** captures this field
6. According to **Faraday’s Law**, voltage is induced
7. This voltage is used for **charging EV battery**

---

## 🔌 Circuit Explanation

### 🧩 Transmitter Side (Road)

```cpp
- BD139 Transistor
- Copper Coil (Transmitter)
- Resistor (Base control)
- Power Supply
```

**Connections:**
- Base → Resistor → Signal input  
- Collector → Coil end  
- Emitter → Ground  
- Coil center → Vcc  

👉 BD139 switches ON/OFF rapidly → creates alternating magnetic field

---

### 🚗 Receiver Side (EV)

```cpp
- Copper Coil (Receiver)
- Rectifier (Diodes)
- Filter Capacitor
- Battery / Load
```

👉 Magnetic field → induces AC voltage → converted to DC → charges battery

---

## 🛠️ Hardware Components

- ESP8266 (NodeMCU)
- DHT11 Sensor
- Touch Sensors (3 units)
- 16x2 LCD (I2C)
- BD139 Transistor
- Copper Coils (Transmitter & Receiver)
- Resistors, Capacitors, Diodes
- Power Supply

---

## 💻 Software & Libraries

- Arduino IDE

**Libraries Used:**
- ESP8266WiFi
- ESP8266WebServer
- ESP8266HTTPClient
- ESP_Mail_Client
- DHT
- LiquidCrystal_I2C

---

## 🔌 Pin Configuration

| Component       | Pin |
|----------------|-----|
| DHT11          | D4  |
| Touch Sensor 1 | D5  |
| Touch Sensor 2 | D6  |
| Touch Sensor 3 | D7  |
| LCD (I2C)      | 0x27 |

---

## ⚙️ Configuration

```cpp
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

#define AUTHOR_EMAIL     "your_email@gmail.com"
#define AUTHOR_PASSWORD  "your_app_password"
#define RECIPIENT_EMAIL  "receiver_email@gmail.com"

const char* SHEETS_URL = "YOUR_GOOGLE_SCRIPT_URL";
```

---

## 📊 Working Flow

1. Wind rotates VAWT → generates electricity  
2. ESP8266 monitors sensors  
3. Data is:
   - Displayed on LCD  
   - Sent to web dashboard  
   - Logged in Google Sheets  
4. Generated energy → supplied to transmitter coil  
5. Magnetic field created  
6. EV receiver coil captures energy  
7. Battery charging happens wirelessly  
8. If threat detected:
   - Email alert sent  
   - Dashboard alert triggered  

---

## 🌐 Web Dashboard

- Real-time sensor monitoring  
- Live graphs  
- Threat alerts  
- System status  
- Event logs  

---

## 📧 Email Alert System

- Uses Gmail SMTP
- Sends alerts for:
  - Threat detection  
  - System abnormality  

---

## 📍 Location Tracking

```cpp
const float TURBINE_LAT[] = {9.575062, 9.575180, 9.575310};
const float TURBINE_LON[] = {77.675734, 77.675890, 77.676050};
```

---

## 📈 Data Logging

- Interval: 15 seconds  
- Platform: Google Sheets  
- Data:
  - Temperature  
  - Humidity  
  - Status  
  - Timestamp  

---

## ⚠️ Limitations

- Wireless power transfer efficiency decreases with distance  
- Coil alignment is critical  
- Energy loss occurs due to air gap  

---

## 🔮 Future Improvements

- Increase efficiency using resonant coupling  
- Add battery management system  
- Integrate AI-based predictive maintenance  
- Improve coil design for longer range  

---

## 🎯 Applications

- Smart Highways  
- Wireless EV Charging Stations  
- Renewable Energy Systems  
- Smart Cities  

---

## 👨‍💻 Team

- Nagella Nagavenkat  
- Kommineni Kavya Sree  
- Maruprolu Jyothsna  
- Munugu Tejasree  
- Kumaran Gobika  

---

## 📄 License

This project is for academic and research purposes.
