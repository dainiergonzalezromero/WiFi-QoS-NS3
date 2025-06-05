# 🧠 PoFi-SDN-WiFi: Simulation of a Cognitive Access Point with SDN and QoS (EDCA)

This project implements a simulation environment in NS-3 that combines SDN (Software Defined Networking) concepts with IEEE 802.11 WiFi networks using **QoS based on EDCA (Enhanced Distributed Channel Access)**. It focuses on the creation of an intelligent access point (`PoFiAp`) which, together with an SDN controller (`PoFiController`), allows:

- Classification of packets according to priority (ToS/DSCP).
- Queuing by traffic type.
- Dynamic allocation of **TXOP**.
- Collection of detailed metrics by class of service.
- Application of SDN policies via `PacketIn`/`FlowMod` messages.

---

## 🎯 Objectives

- Simulate a cognitive WiFi ecosystem with SDN.
- Analyze differentiated traffic behavior (VoIP, video, Best Effort, Background).
- Evaluate key metrics: **Throughput**, **Delay**, **Lost Packets**.

---

## 📡 PoFiAp (Intelligent Access Point)

The `PoFiAp` is a derived class that extends the behavior of a WiFi access point to make it **QoS-aware and cognitive**:

### Main functions:

- **Classification**: analyzes the ToS/DSCP field of each received packet.
- **Access Category (AC) assignment**:
  - **VO (Voice)**: highest priority.
  - **VI (Video)**.
  - **BE (Best Effort)**.
  - **BK (Background)**: lowest priority.

- **Queues by AC**: maintains separate buffers for each category.
- **Dynamic TXOP**: calculates how much time each AC is allowed to transmit based on current traffic.
- **Metrics collection**:
  - Delay time per packet.
  - Packets lost per queue.
  - Cumulative throughput.
- **CSV log generation** for later analysis.

---

## 🧠 PoFiController (SDN Controller)

The `PoFiController` interacts with the `PoFiAp` through `PacketIn` events. It responds with `FlowMod` rules to dictate:

- Which queue to direct the packet to.
- Whether forwarding should be allowed.
- Parameters such as the recommended TXOP.

### Logic:

1. Receives packets from `PoFiAp`.
2. Inspects the ToS to determine the type of traffic.
3. Returns instructions with `FlowMod` for processing.

---

## 🎮 How is EDCA behavior simulated?

### EDCA: Enhanced Distributed Channel Access

EDCA is the QoS mechanism in 802.11e that allows traffic differentiation using 4 **Access Categories** (AC):

| Access Category | Typical traffic     | Priority | Containment | Typical TXOP |
|-----------------|--------------------|-----------|------------|-----------|
| VO              | Voice, VoIP          | Very high  | Minimal     | Long     |
| VI              | Video              | High      | Low       | Medium     |
| BE              | Browsing, email | Medium     | Medium      | Low      |
| BK              | Transfers     | Low      | High       | Short     |

In this simulation:

- Different queues are assigned for each AC.
- The `PoFiAp` queues and schedules the transmission according to its category.
- The `PoFiController` can dynamically adjust the **TXOP** of each queue based on the observed traffic.

---

## 📈 Metrics collected

For each access category and packet size:

- **Average delay (ms)**.
- **Cumulative throughput (kbps)**.
- **Number of lost packets**.

These metrics are saved in CSV files for each combination of:

- Packet size: `256, 512, 1024`.
- Traffic category: `VO, VI, BE, BK`.

---

## 🚀 Execution

```bash
./waf --run scratch/sdwn
```
## Configurable parameters:
- Number of stations.
- Packet size.
- Simulation duration.
- TXOP allocation algorithm.

## 📁 Project structure
scratch/Finals/
├── sdwn.cc                 # Main simulation
├── flowmon/                # Node statistics by service class
├── xml/                    # Visualization of simulations by service class
├── statistics/           # Results in CSV by service class
└── pcap/                   # Packet capture by service class

## 🛠 Requirements
- NS-3 (verified in version 3.44)

## 📌 Additional notes
This simulation can be extended to include ML-based decisions (e.g., traffic pattern learning).
The architecture is ideal for centralized control experiments in IoT environments, edge computing, etc.

## 👨‍💻 Author
Developed by Dainier Gonzalez Romero. Institute of Computer Science and Engineering (ICIC) - National Council for Scientific and Technical Research (CONICET) - National University of the South (UNS)

Doctoral thesis: Design and development of digital ecosystems for cognitive IoT. Software-defined networks as a planning tool
