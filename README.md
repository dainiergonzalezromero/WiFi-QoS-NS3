# 🧠 CA-SDWN-WiFi-QoS: AI-Driven Knowledge-Defined Wireless Networks (KDWN) 
<p align="center">
  <img src="PDF_Graficas/Captura desde 2026-03-06 18-43-10.png"width="600">
</p>

This project implements an advanced **simulation and optimization framework** in NS-3 that combines **Software Defined Networking (SDN)**, **IEEE 802.11e WiFi QoS (EDCA)**, and **Machine Learning** to create a **Knowledge-Defined Wireless Network (KDWN)**.

It extends NS-3's WiFi module to model a **Cognitive Access Point (`PoFiAp`)** interacting with an **SDN Controller (`KDNController`)**. Furthermore, it integrates an **Intelligent Agent** that uses **Random Forest** and **Bayesian Optimization** to automatically find optimal network parameters (Contention Windows) to minimize latency and packet loss while maximizing throughput. The project includes a complete **Python-based automation suite for massive simulation execution, data processing, statistical analysis, and professional visualization**.

---

## 🎯 Objectives

* Simulate a **QoS-aware** and **SDN-controlled WiFi environment** with differentiated traffic categories (`Voice`, `Video`, `Best Effort`, `Background`)
* Implement a centralized **SDN controller(`KDNController`)** that dynamically manages **EDCA parameters** based on network conditions
* Create a **cognitive Access Point (`PoFiAp`)** with per-priority queuing and real-time metric collection
* Evaluate **Throughput**, **Delay**, and **Packet Loss** per access category across multiple network densities (10-100 nodes)
* Predict network behavior using a multi-output **Random Forest Regressor** (24 simultaneous predictions)
* Optimize network parameters (CWmin/CWmax) automatically using **Bayesian Optimization (`TPE`)** with **Optuna**
* Automate massive parameter exploration (1000+ unique configurations, 10 repetitions each)
* Generate publication-quality graphs with confidence intervals (95%, t-student)
* Provide a complete baseline comparison between traditional WiFi (`NO SDWN`) and SDN-enabled architectures

---

## 📡 PoFiAp (Intelligent Access Point)

The `PoFiAp` class extends a standard WiFi AP to become **cognitive and SDN-controllable**, allowing real-time traffic classification and queue management.

### ✳️ Main functions:
- **Classification**: inspects the ToS/DSCP field of incoming packets to determine traffic type.
- **Access Category assignment (EDCA)**:
   * **VO (Voice)**: highest priority (ToS `0xe0`)
   * **VI (Video)**: medium priority (ToS `0xa0`)
   * **BE (Best Effort)**: low priority (ToS `0x60`)
   * **BK (Background)**: no priority (ToS `0x20`)
- **Per-AC Queues**  with strict priority scheduling (HIGH > MEDIUM > LOW > NRT).
- **Dynamic EDCA configuration** adjusts AIFSN, CWmin, CWmax, and TXOP limits per AC.
- **PacketIn/FlowMod communication** with the SDN controller
- **Real-time metric collection** per category: delay, jitter, throughput, packet loss
- **CSV logging** for post-simulation analysis

---

## 🧠 KDNController (SDN Controller)

The `KDNController` communicates with the PoFiAp through SDN messages (`PacketIn` and `FlowMod`), dictating forwarding decisions and QoS rules.

### ⚙️ Logic
1. Receives a `PacketIn` from PoFiAp with packet metadata (ToS, source IP)
2. Analyzes ToS/DSCP to determine traffic type and priority requirements.
3. Issues `FlowMod` rules specifying:
   - Queue assignment (HIGH/MEDIUM/LOW)
   - Recommended TXOP limit (in microseconds)
   - Priority classification for future packets from that source
4. Maintains a registry of flow rules for efficient processing
---

## 🤖 Intelligent Agent (Knowledge Plane)

The system includes a Python-based **AI Agent** (`Inteligen_Agent.py`) that acts as the **Knowledge Plane**, closing the optimization loop.

### ⚙️ Workflow
1. **Data Ingestion**: Reads consolidated simulation results from a file.
2. **Feature Engineering**:Uses 14 features including packet size, node distribution (nStaH/M/L/NRT), and Contention Window parameters (CWmin/CWmax for all ACs).
3. **Multi-output Modeling**: Trains a **Random Forest Regressor** (500 estimators) to predict 24 targets simultaneously:
* Throughput mean/std for H/M/L/NRT
* Delay mean/std for H/M/L/NRT
* Lost packets mean/sum for H/M/L/NRT
4. **Hyperparameter Optimization**: RandomizedSearchCV (300 iterations) with 5-fold cross-validation.
5. **Bayesian Optimization with Optuna**: Finds optimal CWmin/CWmax values minimizing the objective function:
   
   $$Score = 0.35(D_H + L_H) + 0.35(D_M + L_M) + 0.30(D_L + L_L)$$


   Where each term combines normalized delay and packet loss metrics.
6. **Validation**: Tests optimal configurations with real simulations (`03_sdwn_ia_comprobacion.py`).
---

## 🔄 Dynamic Parameter Update Mechanism

The system features a real-time **parameter update mechanism** that reacts to network topology changes (e.g., when a node connects or disconnects).

### ⚙️ Event-Driven Workflow:
1.  **Event Detection:** The `PoFiAp` detects detects a station connection/disconnection and updates its internal node count per priority ($N_H, N_M, N_L, N_{NRT}$).
2.  **Update Request:** The AP sends an `UpdateRequest` message to the `KDNController` with the new topology.
3.  **Optimization Call:** The Controller forwards the new network state to the **Intelligent Agent**.
4.  **Re-Calculation:** The Agent uses its pre-trained Random Forest model to predict optimal Contention Window ($CW_{min}, CW_{max}$) values for the current topology.
5.  **Parameter Deployment:** The Agent returns the values to the Controller, which sends a `ParameterUpdate` message to the AP.
6.  **Network Broadcast:** The AP applies the new EDCA parameters to its queues and broadcasts them to all connected stations via beacon frames.

---

## ⚡ EDCA Behavior Simulation

EDCA (Enhanced Distributed Channel Access) is the IEEE 802.11e mechanism for **differentiated medium access**.
The simulation models the four **Access Categories (AC)** as independent queues with different contention parameters:

| Access Category | Traffic Type | Priority | AIFSN | CWmin | CWmax | TXOP (μs)|
|-----------------|--------------|----------|-------|-------|-------|----------|
VO (Voice)  |  VoIP, real-time   |  HIGH  |  2  |  3  |  7  |  1504  |
VI (Video)  |  Streaming   |  MEDIUM   |  2  |  7  |  15 |  3008  |
BE (Best Effort)  |  Web, email  |	LOW	|  3	|  15 |	1023  |	0  |
BK (Background)   |  File transfer  |	NRT   |  3  |	15 |	1023  |	0  |

The `PoFiAp` schedules packets with strict priority, while the `KDNController` can dynamically adjust `TXOP` and contention parameters during runtime. The **Intelligent Agent** finds optimal CW values to maintain **low latency and minimal packet loss** even in dense networks (up to 100 nodes).

---

## 📈 Metrics Collected

For each simulation run and per Access Category:

|  Metric	|  Description |	Unit  |
|-----------|--------------|--------|
Throughput  |	Data rate successfully delivered |	Kbps |
Delay |	End-to-end packet latency  |	ms |
Jitter   |	Delay variation   |	ms |
Lost Packets   |	Number of packets dropped  |	count |
Packet Loss Rate  |	Percentage of lost packets |	%  |
Sent Packets   |	Total packets transmitted  |	count |
Received Packets  |	Total packets successfully received |	count |

All metrics are exported to CSV with full parameter context (node count, packet size, CW values) for post-processing.

---

## 🧩 Project Structure

The project follows a comprehensive hierarchical structure to organize simulations, source code, automation scripts, and the massive datasets generated by experiments.

```
scratch/
│
├── Statistics/                                    # RAW RESULTS HIERARCHY
│   ├── SDWN-EDCA/                                 # SDN-enabled simulations with EDCA
│   │   └── 1S/                                    # Packet interval: 1 second
│   │       ├── 10/ ... 100/                       # Network density (nodes)
│   │       │   ├── 256/                           # Packet size: 256 bytes
│   │       │   │   ├── SDWN-EDCA_...Run1.csv      # Raw metrics per run
│   │       │   │   ├── ... (10 runs)
│   │       │   │   └── SDWN-EDCA_...Run10.csv
│   │       │   ├── 512/                           # Packet size: 512 bytes
│   │       │   └── 1024/                          # Packet size: 1024 bytes
│   │       └── Logs/                              # NS-3 execution logs
│   │
│   ├── NO-SDWN/                                   # Baseline simulations (no SDN)
│   │   └── [Same hierarchy as SDWN-EDCA]
│   │
│   ├── SDWN-IA/                                   # AI training data
│   │   └── [Same hierarchy with varied CW parameters]
│   │
│   └── Results_Finals/                            # Consolidated results
│       ├── All Data Results.csv                   # Full concatenated dataset
│       └── Resultados_Finales.csv                 # Statistical summaries
│
├── FINAL_GRAPHS/                                  # VISUALIZATION ASSETS
│   ├── graficar.py                                # Interactive plotting script
│   └── RESULTADO_FINAL.xlsx                       # Master Excel with all results
│
├── AI/                                            # ARTIFICIAL INTELLIGENCE PIPELINE
│   ├── 01_concatenate_results.py                  # Merges raw simulation CSVs
│   ├── 02_estadisticas.py                         # Calculates statistics (mean, std, CI)
│   ├── 03_sdwn_ia_comprobacion.py                 # Validates optimal configurations
│   ├── Inteligen_Agent.py                         # Bayesian optimization with Optuna
│   ├── modelo_caracteristicas.py                  # Feature engineering utilities
│   └── RF Model.py                                # Random Forest training
│
├── Master_Model/                                  # TRAINED MODELS & ANALYTICS
│   ├── comparison_Delay_H_mean.png                # Delay comparison: HIGH priority
│   ├── comparison_Delay_M_mean.png                # Delay comparison: MEDIUM priority
│   ├── comparison_LostPackets_H_mean.png          # Packet loss: HIGH priority
│   ├── comparison_LostPackets_M_mean.png          # Packet loss: MEDIUM priority
│   ├── feature_importance.csv                     # Feature ranking from RF model
│   └── Modelo Optimizado.joblib                   # Final trained model
│
├── NO_SDWN/                                       # BASELINE SCRIPTS
│   ├── 01_no_sdwn_paralrell.py                    # Parallel execution for baseline
│   ├── 02_concatenate_results.py                  # Merges baseline CSVs
│   ├── 02_estadisticas.py                         # Statistics for baseline
│   └── no_sdwn.cc                                 # C++ source: traditional WiFi
│
├── SDWN/                                          # SDN-ENABLED SCRIPTS
│   ├── 01_sdwn_comprobacion.py                    # Validation runs
│   ├── 01_sdwn_generate_data_ia.py                # AI training data generation
│   ├── 02_concatenate_results.py                  # Merges SDWN CSVs
│   ├── 02_estadisticas.py                         # Statistics for SDWN
│   └── sdwn.cc                                    # C++ source: SDN controller
│
├── PDF_Graphs/                                    # PUBLICATION-READY GRAPHICS
│   ├── Delay_256_HIGH.pdf                         # Delay for 256B, HIGH priority
│   ├── Delay_512_MEDIUM.pdf                       # Delay for 512B, MEDIUM priority
│   ├── LostPackets_1024_LOW.pdf                   # Loss for 1024B, LOW priority
│   └── ... (36 PDFs total)
│
└── AI_Optimization_Results/                       # OPTIMIZATION OUTPUTS
    ├── Optimizacion_CW_Final.csv                  # Optimal CW values (CSV)
    └── Optimizacion_CW_Final.xlsx                 # Optimization report (Excel)
```
---

## 🚀 Execution Workflow

### 1️⃣ Single Simulation Run
```bash
./ns3 run "scratch/sdwn.cc --nStaH=15 --nStaM=15 --nStaL=15 --nStaNRT=15 --CwMinH=63 --CwMaxH=1023 --CwMinM=7 --CwMaxM=255 --CwMinL=15 --CwMaxL=1023 --CwMinNRT=31 --CwMaxNRT=1023 --PacketSize=512 --TimeSimulationMin=3 --nCorrida=1 --mobilityType=mixer"
```
#### Key parameters:
   * `nStaH/nStaM/nStaL/nStaNRT`: Node distribution per priority (must sum to total nodes)
   * `CwMinX/CwMaxX`: Contention Window parameters for each Access Category
   * `PacketSize`: 256, 512, or 1024 bytes
   * `TimeSimulationMin`: Duration in minutes
   * `mobilityType`: "yes" (mobile), "no" (static), or "mixer" (20% mobile)
   * `nCorrida`: Run number (for reproducibility)

Or

###  2️⃣ Massive Parallel Execution (Data Generation)
```bash
# Generate AI training data (1000+ unique configurations, 10 repetitions each)
python3 SDWN/01_sdwn_generate_data_ia.py

# Run baseline simulations for comparison
python3 NO_SDWN/01_no_sdwn_paralrell.py

# Validate specific configurations
python3 SDWN/01_sdwn_comprobacion.py
```
These scripts use Python's `multiprocessing.Pool` to run simulations in parallel, dramatically reducing total execution time.

###  3️⃣ Data Consolidation and Analysis
```bash
# Merge all individual CSV files into a single dataset
python3 AI/01_concatenate_results.py

# Calculate statistics per configuration group
python3 AI/02_estadisticas.py
```
The output `Resultados_Finales.csv` contains aggregated metrics ready for AI training.

###  4️⃣ Train the Master Model
```bash
# Train Random Forest model (24 outputs) with hyperparameter optimization
python3 AI/RF\ Model.py
```
This generates:
* `Master_Model/Modelo Optimizado.joblib` (trained model + scalers)
* Feature importance ranking
* Diagnostic plots (real vs predicted)

### 5️⃣ Optimize Network Parameters
```bash
# Run Bayesian optimization to find optimal CW values
python3 AI/Inteligen_Agent.py
```
Results saved to `AI_Optimization_Results/Optimizacion_CW_Final.csv`


### 6️⃣ Validate Optimal Configurations
```bash
# Test optimal parameters with real simulations
python3 AI/03_sdwn_ia_comprobacion.py
```
### 7️⃣ Generate Publication-Quality Graphs
```bash
# Interactive mode or batch export
python3 FINAL_GRAPHS/graficar.py
```
Choose option 1 for batch export (36 PDFs) or navigate the interactive menu for custom plots.

## 📊 Results Analysis and Aggregation

### Script:  `02_estadisticas.py`
This script performs sophisticated statistical processing:
- **Input**: All Data Results.csv (consolidated raw data)
- **Processing per configuration group**:
   * Groups by: Packet Size, nStaWifi, nStaH/M/L/NRT, CWminH...CWmaxNRT
   * For each priority (H/M/L/NRT) with stations present:
      - Filters outliers (`LostPackets < 0.99*180`)
      - Applies timeout penalty to delay.
   * Calculates: mean, std, min, max for Throughput and Delay
   * Computes weighted mean packet loss percentage
   * Aggregates total lost and sent packets

$$\text{Delay(ms)} = \frac{(\text{Delay(ms)} \times \text{ReceivedPackets}) + (9 \times 1024 \times 7 / 1000) \times \text{LostPackets}}{\text{SentPackets}}$$

- **Output**: Resultados_Finales.csv with 24 target columns ready for machine learning.

### Example Output Structure
```bash
   Packet Size, nStaWifi, nStaH, nStaM, nStaL, nStaNRT, CWminH, CWmaxH, CWminM, CWmaxM, CWminL, CWmaxL, CWminNRT, CWmaxNRT,
   Throughput_H_mean, Throughput_H_std, Throughput_H_min, Throughput_H_max, Delay_H_mean, Delay_H_std, Delay_H_min, Delay_H_max,
   LostPackets_H_mean, LostPackets_H_sum, ... (repeated for M, L, NRT)
```

### Script: `graficar.py` - Visualization Features
- **Interactive Mode Options**:
1. **Compare Configurations**: Multiple network types (NO SDWN, SDWN EDCA, SDWN MOD, SDWN IA) for one priority
2. **Compare Priorities**: All priorities (H/M/L/NRT) for one configuration
3. **Comparison Matrix**: Subplots combining configurations × priorities

- **Export Mode**:
* Generates 36 PDFs automatically: 3 metrics × 3 packet sizes × 4 priorities
* Each graph includes:
   * All 4 configurations with distinct colors/line styles
   * X-axis: Devices (10-100)
   * Y-axis: Metric value
   * 95% confidence intervals (t-student, n=10, t=2.262)


#### Example output structure:
```
.
├── NO-SDWN/                    # DATA: BASELINE RESULTS (Standard 802.11e)
│   ├── 1S/                     # Packet Interval: 1 Second
│   │   ├── 1024/               # Packet Size: 1024 Bytes
│   │   │   └── 10...90, 100/   # Network Density: Results by Node Count
│   │   ├── 256/                # Packet Size: 256 Bytes
│   │   │   └── 10...90, 100/   # Network Density: Results by Node Count
│   │   └── 512/                # Packet Size: 512 Bytes
│   │       └── 10...90, 100/   # Network Density: Results by Node Count
│   ├── All Data Results.csv    # Full concatenated dataset (Baseline)
│   ├── Resultados_Finales.csv  # Statistical summary (CSV)
│   └── Resultados_Finales.xlsx # Statistical summary (Excel)
│
├── SDWN-EDCA/                  # DATA: SDN WITH STANDARD EDCA CONFIGURATION
│   ├── 1S/                     # Packet Interval: 1 Second
│   │   ├── 1024, 256, 512/     # Packet Size Hierarchy (10 to 100 nodes)
│   ├── All Data Results.csv    # Full concatenated dataset
│   ├── Resultados_Finales.csv  # Statistical summary (CSV)
│   └── Resultados_Finales.xlsx # Statistical summary (Excel)
│
├── SDWN-EDCA-15-1023/          # DATA: SDN WITH FIXED CW (15-1023)
│   ├── 1S/                     # 1-Second Interval Hierarchy
│   │   ├── 1024, 256, 512/     # Packet Size Hierarchy (10 to 100 nodes)
│   ├── All Data Results.csv    # Full concatenated dataset
│   ├── Resultados_Finales.csv  # Statistical summary (CSV)
│   └── Resultados_Finales.xlsx # Statistical summary (Excel)
│
├── SDWN_IA/                    # DATA: INTELLIGENT AGENT TRAINING DATA
│   ├── 1S/                     # 1-Second Interval Hierarchy (10 to 100 nodes)
│   └── Results_Finals/         # Consolidated training summaries
│       ├── All Data Results.csv
│       ├── Resultados_Finales.csv
│       └── Resultados_Finales.xlsx
│
└── SDWN-IA-EVALUATION/         # DATA: FINAL AI MODEL EVALUATION RESULTS
    ├── 1S/                     # 1-Second Interval Hierarchy (10 to 100 nodes)
    ├── All Data Results.csv    # Master evaluation dataset
    ├── Resultados_Finales.csv  # Statistical summary (CSV)
    └── Resultados_Finales.xlsx # Statistical summary (Excel)
```

---

## ⚙️ Requirements

### NS-3 Environment
* **NS-3** (tested on version 3.44, 3.45, 3.46.1)
* C++17 compatible compiler
* Build system: CMake ≥ 3.10

### Python Environment
* **Python ≥ 3.8**
- **Required packages:**:
```bash
  pip install pandas numpy scikit-learn optuna joblib matplotlib openpyxl tqdm psutil
```
  (Optional: `tkinter` preinstalled in most distributions)

### Hardware Recommendations
* **For massive simulations**: Multi-core CPU (16+ cores recommended)
* **RAM**: 16GB+ for parallel execution
* **Storage**: 10GB+ for generated datasets

---

## 📚 Academic Context

Developed as part of the **Doctoral Thesis**:
> “Design and Development of Digital Ecosystems for Cognitive IoT:
> Software-Defined Networks as a Planning Tool”

Related Publications:

> [1] **González Romero, D.; Ochoa, S.F.; Santos, R.** (2026). *"Context-Aware Software-Defined Wireless Networks: An AI-based Approach to Deal with QoS"*. Future Internet (MDPI).

**Author:**

> 🧑‍💻 *Dainier González Romero* <br>
> Telecommunications and Electronics Engineer <br>
> Instituto de Ciencias e Ingeniería de la Computación (ICIC). <br>
> Departamento de Ingeniería Eléctrica y Computadoras (DIEC). <br>
> Universidad Nacional del Sur. <br>
> Bahía Blanca, Buenos Aires, Argentina.
---


## 📜 License

MIT License — free to use, modify and distribute with attribution.
