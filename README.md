# 🧠 PoFi-SDN-WiFi: Simulation of a Cognitive Access Point with SDN and QoS (EDCA)

This project implements an advanced **simulation and analysis framework** in NS-3 that combines **Software Defined Networking (SDN)** and **IEEE 802.11e WiFi QoS (EDCA)** concepts.  
It extends NS-3’s WiFi module to model a **Cognitive Access Point (`PoFiAp`)** interacting with an **SDN Controller (`PoFiController`)**, and now includes a full **Python-based automation and results analysis suite**.

---

## 🎯 Objectives

- Simulate a **QoS-aware and SDN-controlled WiFi environment**.  
- Analyze differentiated traffic categories (Voice, Video, Best Effort, Background).  
- Evaluate **Throughput**, **Delay**, and **Packet Loss** per access category.  
- Automate **massive parameter exploration** (CWmin/CWmax, number of stations).  
- Generate detailed CSV summaries and aggregate reports.

---

## 📡 PoFiAp (Intelligent Access Point)

The `PoFiAp` class extends a standard WiFi AP to become **cognitive and SDN-controllable**, allowing real-time traffic classification and queue management.

### ✳️ Main functions:
- **Classification**: inspects the ToS/DSCP field of incoming packets.  
- **Access Category assignment (EDCA)**:  
  - VO (Voice): highest priority.  
  - VI (Video).  
  - BE (Best Effort).  
  - BK (Background): lowest priority.  
- **Per-AC Queues** with dynamic scheduling.  
- **Dynamic TXOP adjustment** according to observed traffic.  
- **Metric collection** per category: delay, loss, throughput.  
- **CSV logging** for post-simulation analysis.

---

## 🧠 PoFiController (SDN Controller)

The `PoFiController` communicates with the PoFiAp through SDN messages (`PacketIn` and `FlowMod`), dictating forwarding decisions and QoS rules.

### ⚙️ Logic
1. Receives a `PacketIn` from PoFiAp.  
2. Analyzes ToS/DSCP to determine traffic type.  
3. Issues `FlowMod` rules specifying:
   - Queue assignment.  
   - Forwarding decision.  
   - Recommended TXOP.

---

## ⚡ EDCA Behavior Simulation

EDCA (Enhanced Distributed Channel Access) is the IEEE 802.11e mechanism for **differentiated medium access**.  
The simulation models the four **Access Categories (AC)** as independent queues with different contention parameters:

| Access Category | Typical traffic | Priority | Contention | Typical TXOP |
|-----------------|-----------------|-----------|-------------|---------------|
| VO | Voice / VoIP | Very High | Minimal | Long |
| VI | Video | High | Low | Medium |
| BE | Browsing / Email | Medium | Medium | Short |
| BK | File Transfer | Low | High | Very Short |

The `PoFiAp` schedules packets accordingly, while the `PoFiController` can dynamically adjust TXOP and contention parameters during runtime.

---

## 📈 Metrics Collected

For each Access Category and simulation setup:

- Average **delay (ms)**  
- Average **throughput (kbps)**  
- **Lost packets** count  
- Aggregated results are exported to CSV for post-processing

---

## 🧩 Project Structure

```
scratch/
│
├── sdwn.cc                # Main NS-3 simulation (PoFiAp + PoFiController)
├── sdwn.py                # Sequential simulation runner
├── sdwn_parallel.py       # Parallel (multi-core) simulation runner
├── results/               # Output CSVs per experiment
│   ├── Run_XX.csv
│   └── ...
└── analysis/
    └── analyze_results.py  # Python tool to consolidate and aggregate results
```

---

## 🚀 Execution Workflow

### 1️⃣ Run the simulation in NS-3
```bash
./ns3 run "scratch/sdwn --nStaH=15 --nStaM=15 --nStaL=15 --nStaNRT=15 --CwMinH=63 --CwMaxH=1023 --CwMinM=7 --CwMaxM=255 --CwMinL=15 --CwMaxL=1023 --CwMinNRT=31 --CwMaxNRT=1023 --PacketSize=512 --TimeSimulationMin=3 --nCorrida=3"
```

Parameters configurable in the script:
- Number of stations (per priority class)
- Packet size (256, 512, 1024)
- Simulation duration
- TXOP and contention parameters (CWmin, CWmax)

---

### 2️⃣ Automate experiments with Python

#### Sequential mode
```bash
python3 sdwn.py &
```
Runs all parameter combinations one after another.

#### Parallel mode (recommended)
```bash
python3 sdwn_parallel.py &
```
Executes multiple NS-3 instances concurrently using multiprocessing — ideal for large experimental batches.

---

## 📊 Results Analysis and Aggregation

### Script: `analyze_results.py`

After running simulations, use this script to:
1. **Select a CSV file** interactively (Tkinter GUI).
2. Automatically detect all experiment files in the same folder.
3. Merge, clean, and normalize simulation data.
4. Add context columns:
   - `TotalDevices`, `nStaH`, `nStaM`, `nStaL`, `nStaNRT`
   - `Prioridad` (HIGH, MEDIUM, LOW, NRT)
5. Generate:
   - ✅ `Updated/` folder with enriched individual CSVs  
   - ✅ `01_Todos_Concatenados.csv` — all devices combined  
   - ✅ `02_Resumen_Por_Prioridad.csv` — grouped by CW configuration and priority, with average throughput, delay, and loss rate  

#### Example output structure:
```
/Results Finals/
├── simulations_summary.csv
├── Updated/
│   ├── Exp_CWMin(4-8-16-32)_CWMax(8-16-32-64).csv
│   ├── ...
├── 01_Todos_Concatenados.csv
└── 02_Resumen_Por_Prioridad.csv
```

The final summary includes the following columns:

| Category | Metric | Example Column |
|-----------|---------|----------------|
| High Priority | Throughput | `Throughput_H` |
| Medium Priority | Delay | `Delay_M` |
| Low Priority | Packet Loss | `Loss_Packets_L` |
| Non-Real-Time | Throughput | `Throughput_noRT` |

---

## ⚙️ Requirements

- **NS-3 (tested on version 3.44)**
- **Python ≥ 3.8**
- **Dependencies**:
  ```bash
  pip install pandas
  ```
  (Optional: `tkinter` preinstalled in most distributions)

---

## 🧪 Example: Complete Workflow

```bash
# Step 1 – Run all simulations in parallel
python3 sdwn_parallel.py

# Step 2 – Analyze results and generate summaries
python3 analysis/analyze_results.py
```

Results:
- Individual experiment CSVs → `/results/`
- Aggregated data → `/results/Updated/`
- Summary reports → `01_Todos_Concatenados.csv`, `02_Resumen_Por_Prioridad.csv`

---

## 📚 Academic Context

Developed as part of the **Doctoral Thesis**:  
> “Design and Development of Digital Ecosystems for Cognitive IoT:  
> Software-Defined Networks as a Planning Tool”

**Author:**  
🧑‍💻 *Dainier Gonzalez Romero*  
Institute of Computer Science and Engineering (ICIC)  
National Council for Scientific and Technical Research (CONICET)  
National University of the South (UNS)

---

## 📜 License

MIT License — free to use, modify and distribute with attribution.
