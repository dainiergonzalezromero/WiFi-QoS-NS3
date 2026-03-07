# 🧠 PoFi-SDN-WiFi: AI-Driven KDWN with SDN and QoS (EDCA)
<p align="center">
  <img src="PDF_Graficas/Captura desde 2026-03-06 18-43-10.png"width="600">
</p>
This project implements an advanced **simulation and optimization framework** in NS-3 that combines **Software Defined Networking (SDN)**, **IEEE 802.11e WiFi QoS (EDCA)**, and **Machine Learning** to create a **Knowledge-Defined Wireless Network (KDWN)**.

It extends NS-3’s WiFi module to model a **Cognitive Access Point (`PoFiAp`)** interacting with an **SDN Controller (`KDNController`)**. Furthermore, it integrates an **Intelligent Agent** that uses **Random Forest** and **Bayesian Optimization** to automatically find the optimal network parameters to minimize latency and maximize throughput. Includes a full **Python-based automation and results analysis suite**.

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

## 🧠 KDNController (SDN Controller)

The `KDNController` communicates with the PoFiAp through SDN messages (`PacketIn` and `FlowMod`), dictating forwarding decisions and QoS rules.

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
2.  **Update Request:** The AP sends an `UpdateRequest` message to the `KDNController`.
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

The `PoFiAp` schedules packets accordingly, while the `KDNController` can dynamically adjust TXOP and contention parameters during runtime, and the `Intelligent Agent` dynamically adjusts parameters to maintain **near-zero packet loss** even in dense networks.

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
├── GRAFICAS FINALES/           # FINAL VISUALIZATION ASSETS
│   ├── graficar.py             # Script to generate comparative performance plots
│   └── RESULTADO_FINAL.xlsx    # Consolidated Excel workbook with all results
│
├── IA/                         # ARTIFICIAL INTELLIGENCE PIPELINE
│   ├── 01_concatenate_results.py    # Data merging from raw simulation outputs
│   ├── 02_estadisticas.py           # Descriptive statistics and CI calculations
│   ├── 03_sdwn_ia_comprobacion.py   # In-simulation verification of the AI agent
│   ├── Inteligen_Agent.py           # Core Reinforcement Learning agent logic
│   ├── modelo_caracteristicas.py    # Feature engineering and data preprocessing
│   └── RF Model.py                  # Random Forest model implementation
│
├── Master_Model/               # TRAINED MODELS & PERFORMANCE ANALYTICS
│   ├── comparacion_Delay_H_mean.png        # Delay comparison: HIGH priority
│   ├── comparacion_Delay_M_mean.png        # Delay comparison: MEDIUM priority
│   ├── comparacion_LostPackets_H_mean.png  # Packet loss comparison: HIGH priority
│   ├── comparacion_LostPackets_M_mean.png  # Packet loss comparison: MEDIUM priority
│   ├── feature_importance.csv              # KPI ranking from the RF model
│   └── Modelo Optimizado.joblib            # Final hyperparameter-tuned ML model
│
├── NO_SDWN/                    # BASELINE (Standard 802.11e EDCA)
│   ├── 01_no_sdwn_paralrell.py      # Parallel execution script for baseline
│   ├── 02_concatenate_results.py    # Merging of baseline raw CSVs
│   ├── 02_estadisticas.py           # Statistical processing for baseline
│   └── no_sdwn.cc                   # C++ Source: Traditional Wi-Fi simulation
│
├── SDWN/                       # PROPOSED SOLUTION (SDN-Enabled)
│   ├── 01_sdwn_generate_data_ia.py  # Simulation runner for AI training data
│   ├── 01_sdwn_comprobacion.py      # Functionality tests for SDWN logic
│   ├── 02_concatenate_results.py    # Merging of SDWN raw CSVs
│   ├── 02_estadisticas.py           # Statistical processing for SDWN
│   └── sdwn.cc                      # C++ Source: Centralized SDN Controller
│
├── PDF_Graficas/               # PUBLICATION-READY VECTOR GRAPHICS
│   ├── Apendice.pdf            # Supplemental data and appendix
│   ├── Delay_XXX_XXX.pdf       # High-res delay plots by packet/priority
│   ├── LostPackets_XXX_XXX.pdf  # High-res loss plots by packet/priority
│   └── Tablas.pdf              # Summary tables for documentation
│
└── Resultados_Optimizacion_IA/ # OPTIMIZATION OUTPUTS
│   ├── Optimizacion_CW_Final.csv    # Optimal Contention Window values
│   └── Optimizacion_CW_Final.xlsx   # Final optimization report in Excel
├── CITATION.cff                # Metadata for citing this research project
├── LICENSE                     # Project license (e.g., MIT, GPL)
└── README.md                   # Main documentation
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

> 🧑‍💻 *Dainier González Romero* <br>
> Ingeniero en Telecomunicaciones y Electrónica. <br>
> Instituto de Ciencias e Ingeniería de la Computación (ICIC). <br>
> Departamento de Ingeniería Eléctrica y Computadoras (DIEC). <br>
> Universidad Nacional del Sur. <br>
> Bahía Blanca, Buenos Aires, Argentina.
---


## 📜 License

MIT License — free to use, modify and distribute with attribution.
