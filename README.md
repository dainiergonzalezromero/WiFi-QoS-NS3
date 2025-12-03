# 🧠 PoFi-SDN-WiFi: AI-Driven KDWN with SDN and QoS (EDCA)

This project implements an advanced **simulation and optimization framework** in NS-3 that combines **Software Defined Networking (SDN)**, **IEEE 802.11e WiFi QoS (EDCA)**, and **Machine Learning** to create a **Knowledge-Defined Wireless Network (KDWN)**.

It extends NS-3’s WiFi module to model a **Cognitive Access Point (`PoFiAp`)** interacting with an **SDN Controller (`PoFiController`)**. Furthermore, it integrates an **Intelligent Agent** that uses **Random Forest** and **Bayesian Optimization** to automatically find the optimal network parameters to minimize latency and maximize throughput. Includes a full **Python-based automation and results analysis suite**.

---

## 🎯 Objectives

- Simulate a **QoS-aware and SDN-controlled WiFi environment**.
- Analyze differentiated traffic categories (Voice, Video, Best Effort, Background).
- Evaluate **Throughput**, **Delay**, and **Packet Loss** per access category.
- **Predict network behavior** using a Random Forest Regressor.
- **Optimize network parameters** (CWmin/CWmax) automatically using Bayesian Optimization (TPE).
- Automate **massive parameter exploration** (CWmin/CWmax, number of stations). 
- Generate detailed CSV summaries and aggregate reports.

---

## 📡 PoFiAp (Intelligent Access Point)

The `PoFiAp` class extends a standard WiFi AP to become **cognitive and SDN-controllable**, allowing real-time traffic classification and queue management.

### ✳️ Main functions:
- **Classification**: inspects the ToS/DSCP field of incoming packets.
- **Access Category assignment (EDCA)**:
  - VO (Voice): highest priority.
  - VI (Video): medium priority.
  - BE (Best Effort): low priority.
  - BK (Background): no priority.
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

## 🤖 Intelligent Agent (Knowledge Plane)

The system includes a Python-based **AI Agent** (`Inteligen_Agent.py`) that acts as the **Knowledge Plane**, closing the optimization loop.

### ⚙️ Logic
1. **Data Ingestion**: Reads consolidated simulation results.
2. **Modeling**: Trains a **Random Forest Regressor** (500 estimators) to predict Throughput, Delay, and Loss based on network state.
3. **Optimization**: Uses **Optuna (TPE Algorithm)** to find the best `CWmin` and `CWmax` parameters that minimize the following objective function:
   $$Score = 0.25(D_H+D_M) + 0.2(L_H+L_M) + 0.05(L_L + L_{NRT})$$
4. **Feedback**: Saves the optimal configuration to `configuraciones.txt` for the next simulation cycle.

---

## 🔄 Dynamic Parameter Update Mechanism

The system features a real-time **UpgradeParameters** mechanism that reacts to network topology changes (e.g., when a node connects or disconnects).

### ⚙️ Event-Driven Workflow:
1.  **Event Detection:** The `PoFiAp` detects a station connection/disconnection and updates its internal count of nodes per priority ($N_H, N_M, N_L, N_{NRT}$).
2.  **Update Request:** The AP sends an `UpdateRequest` message to the `PoFiController`.
3.  **Optimization Call:** The Controller forwards the new network state to the **Intelligent Agent**.
4.  **Re-Calculation:** The Agent uses its pre-trained model to determine the new optimal Contention Window ($CW_{min}, CW_{max}$) values for the current topology.
5.  **Parameter Deployment:** The Agent returns the values to the Controller, which sends a `ParameterUpdate` message to the AP.
6.  **Network Broadcast:** The AP applies the new EDCA parameters to its queues and broadcasts them to all connected stations.

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

The `PoFiAp` schedules packets accordingly, while the `PoFiController` can dynamically adjust TXOP and contention parameters during runtime, and the `Intelligent Agent` dynamically adjusts parameters to maintain **near-zero packet loss** even in dense networks.

---

## 📈 Metrics Collected

For each Access Category and simulation setup:

- Average **delay (ms)**
- Average **throughput (kbps)**
- **Lost packets** count
- Aggregated results are exported to CSV for post-processing

---

## 🧩 Project Structure

The project follows a hierarchical structure to organize simulations, source code, and massive datasets generated by the experiments.

```
scratch/
│
│   Core Simulation & Agents
│   ├── sdwn.cc                 # Main NS-3 simulation (PoFiAp + PoFiController)
│   ├── no_sdwn.cc              # Baseline simulation (Standard WiFi without SDN)
│   ├── Inteligen_Agent.py      # AI Engine: Random Forest + Optuna Optimization
│   └── configuraciones.txt     # Output file for optimized parameters (Agent -> Controller)
│
│   Automation & Analysis
│   ├── sdwn_parallel.py        # Parallel (multi-core) simulation runner
│   ├── sdwn_comprobacion.py    # Simulation integrity validation script
│   ├── analyze_results.ipynb   # Data cleaning and aggregation tool (Jupyter)
│   └── res_unificado.py        # Unified dataset for final reporting
│
├── Estadisticas/ (Results Hierachy)
│   │
│   ├── BE+BK+VI+VO/            # Traffic Scenario
│   │    └── 1S/Modified/        # Interval / Configuration Type
│   │        │
│   │        ├── 10/             # Number of Nodes (e.g., 10 Stations)
│   │        │   ├── 256/        # Packet Size (e.g., 256 Bytes)
│   │        │   │   ├── Results_Finals/
│   │        │   │   ├── Updated/
│   │        │   │   ├── 01_Todos_Concatenados.csv  # Full training dataset
│   │        │   │   ├── 02_Resumen_Por_Prioridad.csv
│   │        │   │   └── SDWN_..._Run1.csv          # Raw simulation metrics
│   │        │   │
│   │        │   ├── 512/        # Packet Size: 512 Bytes
│   │        │   └── 1024/       # Packet Size: 1024 Bytes
│   │        │
│   │        ├── 20/ ... 100/    # Incremental Network Density (up to 100 nodes)
│   │        └── Resumen_Unificado.csv
│   ├── Logs/
│   │   └── SDWN_..._Run1.logs     # NS-3 Execution Logs
│   └── Results_Finals/
│       └── Results_Finals.csv     # Results Finals
│
├── Modelo_Estudios_Logs/       # ML Artifacts
    ├── RF_Model.pkl            # Trained Random Forest Model
    └── Optuna_Study.pkl        # Bayesian Optimization History
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

Or

###  1️⃣ Run the simulation in NS-3 (Data Generation)
```bash
python3 sdwn_parallel.py &
```
Executes multiple NS-3 instances concurrently using multiprocessing to generate training data.

###  2️⃣ Analyze and Aggregate Results
```bash
python3 analyze_results.py
```
Cleans, merges, and prepares the CSV data for the AI agent.

###  3️⃣ Train and Optimize (AI)
```bash
python3 Inteligen_Agent.py
```
Trains the Random Forest model and runs the Bayesian Optimization to find the best network parameters. The result is saved to configuraciones.txt

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
├── Estadisticas/ (Results Hierachy)
│   │
│   ├── BE+BK+VI+VO/            # Traffic Scenario
│   │    └── 1S/Modified/        # Interval / Configuration Type
│   │        │
│   │        ├── 10/             # Number of Nodes (e.g., 10 Stations)
│   │        │   ├── 256/        # Packet Size (e.g., 256 Bytes)
│   │        │   │   ├── Results_Finals/
│   │        │   │   ├── Updated/
│   │        │   │   ├── 01_Todos_Concatenados.csv  # Full training dataset
│   │        │   │   ├── 02_Resumen_Por_Prioridad.csv
│   │        │   │   └── SDWN_..._Run1.csv          # Raw simulation metrics
│   │        │   │
│   │        │   ├── 512/        # Packet Size: 512 Bytes
│   │        │   └── 1024/       # Packet Size: 1024 Bytes
│   │        │
│   │        ├── 20/ ... 100/    # Incremental Network Density (up to 100 nodes)
│   │        └── Resumen_Unificado.csv
│   ├── Logs/
│   │   └── SDWN_..._Run1.logs     # NS-3 Execution Logs
│   └── Results_Finals/
│       └── Results_Finals.csv     # Results Finals
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
  pip install pandas scikit-learn optuna plotly joblib
  ```
  (Optional: `tkinter` preinstalled in most distributions)

---

## 📚 Academic Context

Developed as part of the **Doctoral Thesis**:
> “Design and Development of Digital Ecosystems for Cognitive IoT:
> Software-Defined Networks as a Planning Tool”

and the following research article:

> [1] **González Romero, D.; Ochoa, S.F.; Santos, R.** (2025). *"Context-Aware Software-Defined Wireless Networks: A Proposal Focused on QoS"*. Future Internet (MDPI).

**Author:**

🧑‍💻 *Dainier González Romero*
Ingeniero en Telecomunicaciones y Electrónica.

Instituto de Ciencias e Ingeniería de la Computación (ICIC).

Departamento de Ingeniería Eléctrica y Computadoras (DIEC).

Universidad Nacional del Sur.

Bahía Blanca, Buenos Aires, Argentina.


---


## 📜 License

MIT License — free to use, modify and distribute with attribution.
