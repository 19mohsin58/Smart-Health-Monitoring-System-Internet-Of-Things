# 🏥 Smart Health IoT Monitoring System
### IoT Project 2026 — University of Pisa
**Students:** Mohsin Ali & Andrea Zanin  
**Course:** Internet of Things  
**Professors:** Prof. Righetti & Prof. Vallati

---

## 📋 Table of Contents
1. [Project Overview](#project-overview)
2. [Motivation](#motivation)
3. [System Architecture](#system-architecture)
4. [Application Domain](#application-domain)
5. [Components](#components)
6. [Machine Learning Model](#machine-learning-model)
7. [Communication Protocols](#communication-protocols)
8. [Stress Testing & Adaptation](#stress-testing--adaptation)
9. [Hardware Requirements](#hardware-requirements)
10. [Software Requirements](#software-requirements)
11. [Project Structure](#project-structure)
12. [How to Run](#how-to-run)
13. [Grafana Dashboard](#grafana-dashboard)
14. [References](#references)

---

## 🎯 Project Overview

This project implements a **Smart Health IoT Monitoring System** that simulates
a hospital ward environment where wearable sensor nodes monitor patients'
physiological parameters in real time.

Each sensor node:
- Simulates heart rate readings (60–119 BPM range)
- Runs a **TinyML model locally** to classify readings as normal or anomaly
- Publishes data to the cloud via **MQTT**
- Exposes its status as a **CoAP resource**
- Reacts to **button presses** (patient emergency alerts)
- Uses **LEDs** to indicate system state visually

The cloud application:
- Receives sensor data via MQTT and CoAP
- Stores all data in a **MySQL database**
- Implements **closed-loop control logic** (sends commands back to nodes)
- Provides a **CLI interface** for manual operator control
- Visualizes performance metrics on a **Grafana dashboard**

---

## 💡 Motivation

Continuous patient monitoring is one of the most critical challenges in modern
healthcare. Traditional monitoring systems require patients to be physically
connected to bulky equipment, limiting mobility and comfort. IoT-based wearable
systems offer a compelling alternative:

- **Early anomaly detection** reduces response time in emergencies
- **Edge intelligence** (TinyML) enables autonomous decisions without cloud
  dependency — critical when network connectivity is degraded
- **Low-power wireless networks** (IEEE 802.15.4 / 6LoWPAN) allow long battery
  life for wearable devices
- **Dual-protocol architecture** (MQTT + CoAP) provides both continuous
  telemetry streaming and on-demand resource querying

This project demonstrates how such a system behaves under normal conditions and
under **network stress**, and how it **adapts automatically** to maintain
acceptable performance.

**References for domain inspiration:**
- Gia et al., "IoT-based continuous glucose monitoring system," *Procedia
  Computer Science*, 2017.
- Kakria et al., "A Real-Time Health Monitoring System for Remote Cardiac
  Patients," *Journal of Medical Internet Research*, 2015.

---

## 🏗️ System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                  HOST MACHINE (WSL / Ubuntu)                 │
│                                                             │
│  ┌─────────────┐  ┌─────────────┐  ┌──────────┐ ┌───────┐  │
│  │ Python App  │  │  Mosquitto  │  │  MySQL   │ │Grafana│  │
│  │  (MQTT +    │◄─│   Broker    │  │    DB    │ │  :3000│  │
│  │   CoAP +    │  │   :1883     │  │  :3306   │ │       │  │
│  │   MySQL +   │  └──────▲──────┘  └────▲─────┘ └───▲───┘  │
│  │   CLI)      │         │              │            │       │
│  └──────▲──────┘         │         ┌───┴────────────┘       │
│         │ CoAP           │ MQTT    │ InfluxDB (metrics)     │
└─────────┼────────────────┼─────────┼───────────────────────┘
          │                │         │
┌─────────┼────────────────┼─────────┼───────────────────────┐
│         ▼                ▼         │  tunslip6 TUN Bridge   │
│    ┌─────────────────────────┐     │  fd00::1               │
│    │      Border Router      │─────┘                        │
│    └────────────┬────────────┘                              │
│                 │ IEEE 802.15.4 / 6LoWPAN / RPL             │
│    ┌────────────┴────────────┐                              │
│    │                         │                              │
│ ┌──▼──────────┐  ┌───────────▼─┐  ┌─────────────┐         │
│ │ Sensor Node │  │ Sensor Node │  │   Spammer   │         │
│ │   (Node 2)  │  │   (Node 3)  │  │   (Node 4)  │         │
│ │  Patient 1  │  │  Patient 2  │  │ Stress Test │         │
│ └─────────────┘  └─────────────┘  └─────────────┘         │
│                                                             │
│              COOJA SIMULATION ENVIRONMENT                   │
└─────────────────────────────────────────────────────────────┘
```


---

## 🏥 Application Domain: Smart Health

**Use Case:** Remote Patient Heart Rate Monitoring in a Hospital Ward

**Scenario:**
- Multiple patients wear sensor nodes that continuously monitor heart rate
- Each node runs a local ML model to detect cardiac anomalies autonomously
- A patient can press a button to trigger an **emergency alert** even if vitals
  appear normal (e.g., chest pain, dizziness)
- The cloud application monitors all patients, stores readings, and can send
  commands back (e.g., increase monitoring frequency)
- Medical staff monitor the Grafana dashboard for real-time status

**Simulated Sensor Values:**
Since we use Cooja (simulator), real physical sensors are not attached during
simulation. Sensor readings are generated as follows:

```c
heart_rate = 60 + (rand() % 60);  /* 60–119 BPM */
```

- **Normal range:** 60–100 BPM → GREEN LED
- **Anomaly range:** 101–119 BPM → RED LED + MQTT alert

When deployed on **real nrf52840 dongles**, the professor provides actual sensor
hardware, and these simulated values are replaced by real sensor driver readings.

---

## 🔧 Components

### 1. Sensor Node (`cooja/sensor-node/sensor-node.c`)
The main IoT device firmware written in C for Contiki-NG.

**Responsibilities:**
- Generate/read heart rate values every 5 seconds
- Run TinyML model for local anomaly classification
- Control LEDs based on system state
- React to button press events (patient emergency)
- Connect to MQTT broker via RPL/IPv6 network
- Publish telemetry to MQTT status topic
- Publish emergency alerts to MQTT alert topic
- Publish registration message on startup
- Expose CoAP resource for direct querying
- Implement adaptive behavior under stress

**LED States:**
| LED    | State | Meaning                          |
|--------|-------|----------------------------------|
| GREEN  | ON    | Node running, monitoring normal  |
| RED    | ON    | Anomaly detected or emergency    |
| YELLOW | ON    | Button being pressed by patient  |
| BLUE   | ON    | Command received from cloud      |

**Button Behavior:**
| Event        | Duration  | Action                          |
|--------------|-----------|---------------------------------|
| Press        | Any       | YELLOW LED on (visual feedback) |
| Release      | < 3 sec   | Toggle emergency alert          |
| Long press   | ≥ 3 sec   | Force reset all alerts          |

**MQTT Topics Published:**
| Topic                    | Content                        |
|--------------------------|--------------------------------|
| `health/node/register`   | Node registration info (JSON)  |
| `health/node/status`     | Periodic vitals reading (JSON) |
| `health/node/alert`      | Emergency alert (JSON)         |

**MQTT Topics Subscribed:**
| Topic                    | Content                        |
|--------------------------|--------------------------------|
| `health/node/actuator`   | Commands from cloud            |

**JSON Message Format (status):**
```json
{
  "node": "health-node-a1b2",
  "heart_rate": 87,
  "anomaly": 0,
  "alert": 0
}
```

---

### 2. Border Router (`cooja/border-router/`)
Standard Contiki-NG RPL border router.

**Responsibilities:**
- Acts as RPL DODAG root (network coordinator)
- Bridges the 6LoWPAN wireless network to the host machine
- Provides IPv6 connectivity to all sensor nodes
- Runs `tunslip6` tunnel to host OS

**How it works:**

---

### 3. Spammer Node (`cooja/spammer/`)
A stress-testing node that floods the wireless channel.

**Responsibilities:**
- Sends broadcast packets every 100ms using NullNet
- Creates network congestion to trigger adaptive mechanisms
- Used only during stress test evaluation

---

### 4. Python Backend (`backend/`)

**Files:**
- `mqtt_handler.py` — subscribes to all MQTT topics, stores to MySQL
- `coap_server.py` — CoAP directory server, nodes register here
- `mysql_db.py` — database connection and query functions
- `control_logic.py` — closed-loop control (reads DB, sends commands)
- `influx_writer.py` — writes metrics to InfluxDB for Grafana
- `cli.py` — command line interface for operator

**Closed-Loop Control Logic:**

---

### 5. MySQL Database (`backend/mysql_db.py`)

**Tables:**

```sql
-- Node directory (populated via MQTT register topic)
CREATE TABLE nodes (
  id          INT AUTO_INCREMENT PRIMARY KEY,
  node_id     VARCHAR(50) UNIQUE,
  type        VARCHAR(20),
  domain      VARCHAR(50),
  sensor_type VARCHAR(50),
  registered_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Sensor readings (populated via MQTT status topic)
CREATE TABLE readings (
  id          INT AUTO_INCREMENT PRIMARY KEY,
  node_id     VARCHAR(50),
  heart_rate  FLOAT,
  anomaly     INT,
  alert       INT,
  timestamp   DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Commands sent to nodes (from control logic)
CREATE TABLE commands (
  id          INT AUTO_INCREMENT PRIMARY KEY,
  node_id     VARCHAR(50),
  command     VARCHAR(100),
  sent_at     DATETIME DEFAULT CURRENT_TIMESTAMP
);
```
**Tables:**

```sql
CREATE TABLE nodes (
  id            INT AUTO_INCREMENT PRIMARY KEY,
  node_id       VARCHAR(50) UNIQUE,
  type          VARCHAR(20),
  domain        VARCHAR(50),
  sensor_type   VARCHAR(50),
  registered_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE readings (
  id          INT AUTO_INCREMENT PRIMARY KEY,
  node_id     VARCHAR(50),
  heart_rate  FLOAT,
  anomaly     INT,
  alert       INT,
  timestamp   DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE commands (
  id        INT AUTO_INCREMENT PRIMARY KEY,
  node_id   VARCHAR(50),
  command   VARCHAR(100),
  sent_at   DATETIME DEFAULT CURRENT_TIMESTAMP
);
```
---

### 6. CoAP Server (`backend/coap_server.py`)
Acts as a centralized CoAP directory where nodes register.

**CoAP Resources exposed:**
| Resource         | Methods    | Description                    |
|------------------|------------|--------------------------------|
| `/directory`     | GET, POST  | Node registration directory    |
| `/health/status` | GET        | Current status of all nodes    |
| `/health/alert`  | GET        | Active alerts list             |

**CoAP on Sensor Nodes:**
Each sensor node also runs a CoAP server exposing:
| Resource         | Methods    | Description                    |
|------------------|------------|--------------------------------|
| `/health/status` | GET        | Current node vitals            |
| `/health/alert`  | GET, PUT   | Alert state (readable/settable)|

Nodes also act as **CoAP clients** — they POST their registration info to the
cloud CoAP directory server on startup.

---

## 🤖 Machine Learning Model

### Algorithm: Random Forest Classifier

**Why Random Forest?**
- Lightweight after emlearn conversion — fits in microcontroller RAM
- No floating point math library needed (emlearn generates pure C arrays)
- Good accuracy on tabular health data
- Fast inference — suitable for real-time embedded use
- Professor's lab used this exact approach (Lab 02 Vallati)

**Dataset:**
- Source: Kaggle — [Heart Rate Anomaly Dataset]
  (https://www.kaggle.com/)
- Features used:
  - `heart_rate` (BPM)
  - `spo2` (blood oxygen %)
  - `temperature` (°C)
- Labels:
  - `0` = Normal
  - `1` = Anomaly / Cardiac event

**Training Workflow (Google Colab):**
```python
# 1. Load dataset
df = pd.read_csv('heart_data.csv')
X = df[['heart_rate', 'spo2', 'temperature']].values
y = df['label'].values

# 2. Train/test split
X_train, X_test, y_train, y_test = train_test_split(
    X, y, test_size=0.2, random_state=42)

# 3. Train Random Forest
clf = RandomForestClassifier(
    n_estimators=10,   # small — fits in MCU memory
    max_depth=5,       # small — fits in MCU memory
    random_state=42)
clf.fit(X_train, y_train)

# 4. Evaluate
print("Accuracy:", accuracy_score(y_test, clf.predict(X_test)))

# 5. Convert to C with emlearn
import emlearn
cmodel = emlearn.convert(clf, method='inline')
cmodel.save(file='model.h', name='health_model')
```

**Deployment on Node:**
```c
#include "model.h"

float features[3] = {heart_rate, spo2, temperature};
int prediction = health_model_predict(features, 3);
/* prediction: 0 = normal, 1 = anomaly */
```

**emlearn Makefile Integration:**
```makefile
MODULES_REL += /path/to/emlearn
TARGET_LIBFILES += -lm
INC += /path/to/emlearn
```

---

## 📡 Communication Protocols

### Why MQTT?
MQTT (Message Queuing Telemetry Transport) is ideal for **continuous telemetry
streaming** from IoT devices to the cloud:
- Publish/subscribe model decouples sensors from the application
- Low overhead — minimal header size
- QoS levels allow reliability tuning
- Perfect for periodic heart rate readings (fire and forget)
- Broker (Mosquitto) handles message routing and buffering

**Used for:** Periodic status updates, emergency alerts, node registration,
actuator commands.

### Why CoAP?
CoAP (Constrained Application Protocol) is ideal for **direct resource access**
and **on-demand queries**:
- RESTful model (GET/POST/PUT) — intuitive resource structure
- UDP-based — low overhead on constrained networks
- Observable resources — server-push without polling
- Nodes act as both servers (expose resources) and clients (register to
  directory)

**Used for:** Node directory registration, on-demand status queries, alert state
modification.

### Protocol Assignment Justification:
| Protocol | Role          | Justification                              |
|----------|---------------|--------------------------------------------|
| MQTT     | Telemetry     | High-frequency data streaming, async model |
| CoAP     | Configuration | Low-latency queries, RESTful resource model|

---

## ⚡ Stress Testing & Adaptation

### Stress Scenario: Network Congestion

**Method:** A dedicated "spammer" node (Node 4) is added to the Cooja
simulation. It uses NullNet to broadcast packets every 100ms, flooding the
IEEE 802.15.4 channel and causing:
- Increased packet collision rate
- Higher latency for MQTT messages
- Reduced packet delivery ratio (PDR)
- Missed readings at the cloud application

**Metrics measured:**
- Latency (time between publish and receive)
- Packet Delivery Ratio (PDR)
- Number of messages received per minute at cloud
- Number of anomaly alerts missed

### Adaptive Mechanism 1: Dynamic Rate Adaptation
When an anomaly is detected, the node automatically reduces its transmission
frequency to back off and reduce channel load:

```c
if(heart_rate > 100) {
  /* Anomaly: slow down to reduce congestion */
  send_interval = 20 * CLOCK_SECOND;
  printf("[ADAPT] Rate reduced: 20s interval\n");
} else {
  /* Normal: restore standard rate */
  send_interval = 5 * CLOCK_SECOND;
}
```

**Effect:** Fewer collisions, channel recovers, other nodes can transmit.

### Adaptive Mechanism 2: CoAP Message Type Switching
Under stress, low-priority CoAP messages switch from Non-Confirmable (NON)
to Confirmable (CON) to ensure delivery:

```c
if(anomaly_detected) {
  /* High priority: use CON — broker must acknowledge */
  msg_type = COAP_TYPE_CON;
  printf("[ADAPT] CoAP switched to CON mode\n");
} else {
  /* Normal: use NON — no ack needed, less overhead */
  msg_type = COAP_TYPE_NON;
}
```

**Effect:** Critical anomaly alerts are guaranteed delivery even under load.

### Before vs After Comparison:
Grafana dashboard shows two phases:
1. **Before adaptation** (spammer active, no adaptation): high latency, low PDR
2. **After adaptation** (adaptive mechanisms active): latency drops, PDR
   recovers

---

## 🔌 Hardware Requirements

### For Cooja Simulation (Development & Demo):
- **1 laptop/PC** running Ubuntu 22.04 (or WSL2 on Windows)
- Minimum 8GB RAM recommended
- Java JDK installed (for Cooja)
- No physical hardware needed

### For Real Hardware Deployment (nrf52840 dongles):
The professor provides the following hardware:

| Device            | Quantity | Role                              |
|-------------------|----------|-----------------------------------|
| nrf52840 dongle   | 1        | Border Router (flashed with BR firmware) |
| nrf52840 dongle   | 2+       | Sensor nodes (flashed with sensor firmware) |

**nrf52840 Dongle Specs:**
- CPU: ARM Cortex-M4 @ 64 MHz
- RAM: 256 KB
- Flash: 1 MB
- Radio: IEEE 802.15.4 (2.4 GHz)
- USB: Built-in USB bootloader
- LEDs: 1 RGB LED + 1 Green LED
- Button: 1 user button

**Flashing sensor node on dongle:**
```bash
make TARGET=nrf52840 BOARD=dongle sensor-node.dfu-upload PORT=/dev/ttyACM0
```

**Flashing border router on dongle:**
```bash
make TARGET=nrf52840 BOARD=dongle border-router.dfu-upload PORT=/dev/ttyACM0
```

**Connecting border router dongle to tunslip6:**
```bash
sudo ./tunslip6 -s /dev/ttyACM0 fd00::1/64
```
*(Note: In Cooja simulation, replace `-s /dev/ttyACM0` with `-a 127.0.0.1 -p 60001`)*

---

## 💻 Software Requirements

### WSL / Ubuntu Packages:
```bash
sudo apt install mosquitto mosquitto-clients -y
sudo apt install mysql-server -y
sudo apt install python3-pip -y
sudo apt install default-jdk -y    # for Cooja
```

### Python Libraries:
```bash
pip3 install paho-mqtt aiocoap influxdb-client mysql-connector-python
```

### Contiki-NG:
```bash
git clone https://github.com/contiki-ng/contiki-ng.git
cd contiki-ng
git submodule update --init --recursive
```

### Cooja Simulator:
```bash
cd contiki-ng/tools/cooja
./gradlew run
```

### InfluxDB 2.x:
```bash
wget https://dl.influxdata.com/influxdb/releases/influxdb2-2.7.1-amd64.deb
sudo dpkg -i influxdb2-2.7.1-amd64.deb
sudo systemctl start influxdb
```

### Grafana:
```bash
sudo apt install grafana -y
sudo systemctl start grafana-server
```

---

## 📁 Project Structure

```
iot-project/
│
├── cooja/
│   ├── smart-health.csc
│   ├── sensor-node/
│   │   ├── sensor-node.c
│   │   ├── mqtt-client.h
│   │   ├── project-conf.h
│   │   ├── model.h
│   │   └── Makefile
│   ├── border-router/
│   │   ├── border-router.c
│   │   ├── project-conf.h
│   │   ├── webserver/
│   │   │   ├── httpd-simple.c
│   │   │   ├── httpd-simple.h
│   │   │   └── webserver.c
│   │   └── Makefile
│   └── spammer/
│       ├── spammer.c
│       └── Makefile
│
├── backend/
│   ├── mqtt_handler.py
│   ├── coap_server.py
│   ├── mysql_db.py
│   ├── control_logic.py
│   ├── influx_writer.py
│   └── cli.py
│
├── ml/
│   ├── train_model.ipynb
│   └── model.h
│
└── report/
    └── report.pdf
```

---

## 🚀 How to Run

### Step 1 — Start MySQL
```bash
sudo service mysql start
mysql -u iotuser -p iot_health  # password: iotpass
```

### Step 2 — Start MQTT Broker
```bash
sudo service mosquitto start
```

### Step 3 — Start Cooja Simulation
```bash
cd ~/contiki-ng/tools/cooja
./gradlew run
# File → Open → iot-project/cooja/smart-health.csc
# Press Play button
```

### Step 4 — Start tunslip6 Bridge
```bash
# In a new terminal:
cd ~/contiki-ng/tools/serial-io
sudo ./tunslip6 -a 127.0.0.1 -p 60001 fd00::1/64
```

### Step 5 — Start Python Backend
```bash
# Terminal 1: MQTT handler
cd ~/contiki-ng/iot-project/backend
python3 mqtt_handler.py

# Terminal 2: CoAP server
python3 coap_server.py

# Terminal 3: Control logic + CLI
python3 cli.py
```

### Step 6 — Open Grafana
Browser → http://localhost:3000

Login: admin / admin
### Step 7 — Run Stress Test
```bash
# In Cooja: Add spammer node (Node 4)
# Motes → Add motes → spammer mote type → Add
# Observe Grafana: PDR drops, latency increases
# Adaptive mechanisms activate automatically
# Observe Grafana: system recovers
```

---

## 📊 Grafana Dashboard

**Panels:**
1. **Heart Rate Timeline** — live BPM per node (line chart)
2. **Anomaly Count** — total anomalies per node (bar chart)
3. **Message Rate** — messages/minute received (gauge)
4. **Latency** — end-to-end MQTT latency (line chart)
5. **PDR** — packet delivery ratio % (gauge)
6. **Stress Test Comparison** — before/after adaptive mechanisms (annotated)

**InfluxDB Flux Query Example:**
```flux
from(bucket: "health")
  |> range(start: -30m)
  |> filter(fn: (r) => r._measurement == "patient_vitals")
  |> filter(fn: (r) => r._field == "heart_rate")
```

---

## 📚 References

1. Contiki-NG documentation: https://docs.contiki-ng.org
2. emlearn library: https://github.com/emlearn/emlearn
3. Mosquitto MQTT broker: https://mosquitto.org
4. aiocoap Python library: https://aiocoap.readthedocs.io
5. InfluxDB 2.x: https://docs.influxdata.com
6. Grafana: https://grafana.com/docs
7. Kakria et al., "A Real-Time Health Monitoring System," JMIR, 2015
8. Gia et al., "IoT-based continuous glucose monitoring," Procedia CS, 2017
9. Kaggle Heart Rate Dataset: https://www.kaggle.com
10. University of Pisa IoT Course Slides, Prof. Vallati & Prof. Righetti, 2026