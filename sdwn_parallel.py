#!/usr/bin/env python3
import os
import subprocess
import random
import csv
from datetime import datetime
from multiprocessing import Pool, cpu_count

# ================================
# CONFIGURACIÓN BASE
# ================================
NS3_PATH = "./ns3"
SCRIPT = "scratch/sdwn.cc"

PACKET_SIZE = 512
TIME_SIM_MIN = 3
N_CORRIDAS = 5

TOTAL_DEVICES = [60]
CW_MIN_VALUES = [7, 15, 31, 63]
CW_MAX_VALUES = [15, 31, 63, 255, 1023]

NUM_SIMULATIONS = 20000  # total de simulaciones únicas

os.makedirs("scratch/results", exist_ok=True)
CSV_FILE = "scratch/results/simulations_summary.csv"

# ================================
# FUNCIONES AUXILIARES
# ================================
def generate_sta_distribution(total):
    h_percent = random.randint(0, 4) * 10
    m_percent = random.randint(0, 3) * 10
    l_percent = random.randint(0, 3) * 10
    used = h_percent + m_percent + l_percent
    if used > 100:
        factor = 100 / used
        h_percent = int(h_percent * factor)
        m_percent = int(m_percent * factor)
        l_percent = int(l_percent * factor)
    nrt_percent = max(0, 100 - (h_percent + m_percent + l_percent))
    nStaH = max(0, round(total * h_percent / 100))
    nStaM = max(0, round(total * m_percent / 100))
    nStaL = max(0, round(total * l_percent / 100))
    nStaNRT = total - (nStaH + nStaM + nStaL)
    return nStaH, nStaM, nStaL, nStaNRT

def generate_unique_cw_combinations(num_simulations):
    unique_combos = set()
    while len(unique_combos) < num_simulations:
        cwH = (random.choice(CW_MIN_VALUES), random.choice(CW_MAX_VALUES))
        cwM = (random.choice(CW_MIN_VALUES), random.choice(CW_MAX_VALUES))
        cwL = (random.choice(CW_MIN_VALUES), random.choice(CW_MAX_VALUES))
        cwNRT = (random.choice(CW_MIN_VALUES), random.choice(CW_MAX_VALUES))
        # Validar que cwmax > cwmin
        if all([cwH[1] > cwH[0], cwM[1] > cwM[0], cwL[1] > cwL[0], cwNRT[1] > cwNRT[0]]):
            combo = (cwH, cwM, cwL, cwNRT)
            unique_combos.add(combo)
    return list(unique_combos)

# ================================
# FUNCION PARA EJECUTAR UNA SIMULACION
# ================================
def run_simulation(params):
    sim_id, total, dist_id, dist, cwH, cwM, cwL, cwNRT, corrida = params
    nStaH, nStaM, nStaL, nStaNRT = dist
    CwMinH, CwMaxH = cwH
    CwMinM, CwMaxM = cwM
    CwMinL, CwMaxL = cwL
    CwMinNRT, CwMaxNRT = cwNRT

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    log_name = (
        f"T{total}_Dist{dist_id}_Run{corrida}_"
        f"H{nStaH}M{nStaM}L{nStaL}NRT{nStaNRT}_"
        f"cwH{CwMinH}-{CwMaxH}_cwM{CwMinM}-{CwMaxM}_"
        f"cwL{CwMinL}-{CwMaxL}_cwNRT{CwMinNRT}-{CwMaxNRT}_"
        f"{timestamp}.log"
    )
    log_path = os.path.join("scratch/results", log_name)

    cmd = (
        f'{NS3_PATH} run "{SCRIPT} '
        f'--nStaH={nStaH} --nStaM={nStaM} --nStaL={nStaL} --nStaNRT={nStaNRT} '
        f'--CwMinH={CwMinH} --CwMaxH={CwMaxH} '
        f'--CwMinM={CwMinM} --CwMaxM={CwMaxM} '
        f'--CwMinL={CwMinL} --CwMaxL={CwMaxL} '
        f'--CwMinNRT={CwMinNRT} --CwMaxNRT={CwMaxNRT} '
        f'--PacketSize={PACKET_SIZE} --TimeSimulationMin={TIME_SIM_MIN} --nCorrida={corrida}"'
    )

    with open(log_path, "w") as log_file:
        result = subprocess.run(cmd, shell=True, stdout=log_file, stderr=subprocess.STDOUT)

    if result.returncode != 0:
        return f"[{sim_id}] ❌ Error en simulación"
    else:
        return f"[{sim_id}] ✅ Completada"

# ================================
# GENERAR TODAS LAS SIMULACIONES Y CSV
# ================================
sim_id = 0
params_list = []

# Generar combinaciones únicas
unique_cw_combos = generate_unique_cw_combinations(NUM_SIMULATIONS)

# Crear CSV
with open(CSV_FILE, "w", newline="") as csvfile:
    writer = csv.writer(csvfile)
    header = ["SimID", "TotalDevices", "DistID", "nStaH", "nStaM", "nStaL", "nStaNRT",
              "CwMinH","CwMaxH","CwMinM","CwMaxM","CwMinL","CwMaxL","CwMinNRT","CwMaxNRT","Run"]
    writer.writerow(header)

    for total in TOTAL_DEVICES:
        random_distributions = [generate_sta_distribution(total) for _ in range(NUM_SIMULATIONS)]
        for dist_id, (dist, combo) in enumerate(zip(random_distributions, unique_cw_combos), start=1):
            cwH, cwM, cwL, cwNRT = combo
            for corrida in range(1, N_CORRIDAS + 1):
                sim_id += 1
                params_list.append((sim_id, total, dist_id, dist, cwH, cwM, cwL, cwNRT, corrida))
                nStaH, nStaM, nStaL, nStaNRT = dist
                writer.writerow([sim_id, total, dist_id, nStaH, nStaM, nStaL, nStaNRT,
                                 cwH[0], cwH[1], cwM[0], cwM[1], cwL[0], cwL[1],
                                 cwNRT[0], cwNRT[1], corrida])

print(f"\n📄 CSV de resumen generado: {CSV_FILE}")

# ================================
# EJECUTAR EN PARALELO
# ================================
num_cpus = min(64, cpu_count())  # usar hasta 64 núcleos
print(f"\n💻 Ejecutando simulaciones en paralelo con {num_cpus} núcleos...\n")

with Pool(processes=num_cpus) as pool:
    for result_msg in pool.imap_unordered(run_simulation, params_list):
        print(result_msg)

print("\n🎯 Todas las simulaciones completadas.")
