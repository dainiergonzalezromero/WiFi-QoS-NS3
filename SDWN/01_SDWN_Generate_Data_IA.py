#!/usr/bin/env python3
import os
import subprocess
import random
import csv
from datetime import datetime
from multiprocessing import Pool, cpu_count
from tqdm import tqdm

# ================================
# CONFIGURACIÓN BASE
# ================================
PACKET_SIZES = [256, 512, 1024]
TIME_SIM_MIN = 3
N_CORRIDAS = 5

TOTAL_DEVICES = [10,20,30,40,50,60,70,80,90,100]  # Total de dispositivos por simulación
CW_MIN_VALUES = [3, 7, 15, 31, 63]
CW_MAX_VALUES = [7, 15, 31, 63, 255, 1023]

NUM_SIMULACIONES_UNICAS = 1000  # Configuraciones únicas

# Nuevos parámetros
CATEGORY = "SDWN_IA"
MOBILITY_TYPE = "mixer"  # "yes", "no", o "mixer"
ENABLE_PCAP = False

os.makedirs(f"scratch/Estadisticas/{CATEGORY}/Logs/", exist_ok=True)
os.makedirs(f"scratch/Estadisticas/{CATEGORY}/Results_Finals/", exist_ok=True)
CSV_FILE = f"scratch/Estadisticas/{CATEGORY}/Results_Finals/Simulations_Summary.csv"

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

    nStaH = max(0, round(total * h_percent / 100))
    nStaM = max(0, round(total * m_percent / 100))
    nStaL = max(0, round(total * l_percent / 100))
    nStaNRT = total - (nStaH + nStaM + nStaL)
    return nStaH, nStaM, nStaL, nStaNRT

def generate_unique_cw_combinations(num_simulations):
    unique_combos = set()
    with tqdm(total=num_simulations, desc="Generando combinaciones CW", unit="combo") as pbar:
        while len(unique_combos) < num_simulations:
            cwH = (random.choice(CW_MIN_VALUES), random.choice(CW_MAX_VALUES))
            cwM = (random.choice(CW_MIN_VALUES), random.choice(CW_MAX_VALUES))
            cwL = (random.choice(CW_MIN_VALUES), random.choice(CW_MAX_VALUES))
            cwNRT = (random.choice(CW_MIN_VALUES), random.choice(CW_MAX_VALUES))
            # Validar que cwmax > cwmin
            if all([cwH[1] > cwH[0], cwM[1] > cwM[0], cwL[1] > cwL[0], cwNRT[1] > cwNRT[0]]):
                combo = (cwH, cwM, cwL, cwNRT)
                if combo not in unique_combos:
                    unique_combos.add(combo)
                    pbar.update(1)
    return list(unique_combos)

# ================================
# FUNCIÓN PARA EJECUTAR UNA SIMULACIÓN
# ================================
def run_simulation(params):
    sim_id, packet_size, total, dist_id, dist, cwH, cwM, cwL, cwNRT, corrida, seed = params
    nStaH, nStaM, nStaL, nStaNRT = dist
    CwMinH, CwMaxH = cwH
    CwMinM, CwMaxM = cwM
    CwMinL, CwMaxL = cwL
    CwMinNRT, CwMaxNRT = cwNRT

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    log_name = (
        f"PS{packet_size}_T{total}_Dist{dist_id}_Run{corrida}_"
        f"H{nStaH}M{nStaM}L{nStaL}NRT{nStaNRT}_"
        f"cwH{CwMinH}-{CwMaxH}_cwM{CwMinM}-{CwMaxM}_"
        f"cwL{CwMinL}-{CwMaxL}_cwNRT{CwMinNRT}-{CwMaxNRT}_"
        f"{timestamp}.log"
    )
    log_path = os.path.join(f"scratch/Estadisticas/{CATEGORY}/Logs/", log_name)

    # 1. Usamos la ruta directa al binario compilado
    BIN_PATH = "./build/scratch/ns3.45-sdwn-default"
    
    # Comando actualizado con todos los parámetros
    cmd = (
        f'{BIN_PATH} '
        f'--nStaH={nStaH} --nStaM={nStaM} --nStaL={nStaL} --nStaNRT={nStaNRT} '
        f'--CwMinH={CwMinH} --CwMaxH={CwMaxH} '
        f'--CwMinM={CwMinM} --CwMaxM={CwMaxM} '
        f'--CwMinL={CwMinL} --CwMaxL={CwMaxL} '
        f'--CwMinNRT={CwMinNRT} --CwMaxNRT={CwMaxNRT} '
        f'--PacketSize={packet_size} '
        f'--TimeSimulationMin={TIME_SIM_MIN} '
        f'--nCorrida={corrida} '
        f'--RngSeed={seed} '
        f'--category={CATEGORY} '
        f'--mobilityType={MOBILITY_TYPE} '
        f'--enablePcap={"true" if ENABLE_PCAP else "false"}'
    )

    try:
        with open(log_path, "w") as log_file:
            result = subprocess.run(cmd, shell=True, stdout=log_file, stderr=subprocess.STDOUT, timeout=600)  # timeout de 10 minutos
        
        if result.returncode != 0:
            return f"[{sim_id}] ❌ Error", False
        else:
            return f"[{sim_id}] ✅ Completada", True
    except subprocess.TimeoutExpired:
        return f"[{sim_id}] ⏰ Timeout", False

# ================================
# GENERAR TODAS LAS SIMULACIONES Y CSV
# ================================
print("⚙️  Generando configuraciones...")

sim_id = 0
params_list = []

# Generar combinaciones únicas de CW
print("🔄 Generando combinaciones CW...")
unique_cw_combos = generate_unique_cw_combinations(NUM_SIMULACIONES_UNICAS)

# Crear CSV con más columnas
with open(CSV_FILE, "w", newline="") as csvfile:
    writer = csv.writer(csvfile)
    header = [
        "SimID", "PacketSize", "TotalDevices", "DistID", 
        "nStaH", "nStaM", "nStaL", "nStaNRT",
        "CwMinH", "CwMaxH", "CwMinM", "CwMaxM", 
        "CwMinL", "CwMaxL", "CwMinNRT", "CwMaxNRT", 
        "Run", "Seed", "Category", "MobilityType", "EnablePcap", "Timestamp"
    ]
    writer.writerow(header)
    
    # Calcular total de configuraciones para la barra de progreso
    total_configs = len(TOTAL_DEVICES) * len(PACKET_SIZES) * NUM_SIMULACIONES_UNICAS * N_CORRIDAS
    
    print(f"📊 Generando {total_configs:,} configuraciones en CSV...")
    
    with tqdm(total=total_configs, desc="Generando configuraciones", unit="config") as pbar:
        for total in TOTAL_DEVICES:
            for packet_size in PACKET_SIZES:
                # Generar distribuciones para este total
                random_distributions = [generate_sta_distribution(total) for _ in range(NUM_SIMULACIONES_UNICAS)]
                
                for dist_id, (dist, cw_combo) in enumerate(zip(random_distributions, unique_cw_combos), start=1):
                    cwH, cwM, cwL, cwNRT = cw_combo
                    nStaH, nStaM, nStaL, nStaNRT = dist
                    
                    for corrida in range(1, N_CORRIDAS + 1):
                        sim_id += 1
                        seed = corrida
                        
                        # Agregar a lista de parámetros
                        params_list.append((
                            sim_id, packet_size, total, dist_id, 
                            dist, cwH, cwM, cwL, cwNRT, corrida, seed
                        ))
                        
                        # Escribir en CSV
                        writer.writerow([
                       sim_id, packet_size, total, dist_id,
                            nStaH, nStaM, nStaL, nStaNRT,
                            cwH[0], cwH[1], cwM[0], cwM[1],
                            cwL[0], cwL[1], cwNRT[0], cwNRT[1],
                            corrida, seed, CATEGORY, MOBILITY_TYPE, ENABLE_PCAP,
                            datetime.now().strftime("%Y-%m-%d %H:%M:%S")
                        ])
                        
                        pbar.update(1)

print(f"\n📄 CSV generado: {CSV_FILE}")
print(f"📋 Total de configuraciones: {len(params_list):,}")

# ================================
# EJECUTAR EN PARALELO
# ================================
num_cpus = min(64, cpu_count() - 2)
print(f"\n💻 Ejecutando simulaciones en paralelo con {num_cpus} núcleos...\n")

# Estadísticas de ejecución
exitosas = 0
fallidas = 0
timeouts = 0

with Pool(processes=num_cpus) as pool:
    # Usar imap_unordered con tqdm para mostrar progreso
    resultados = list(tqdm(
        pool.imap_unordered(run_simulation, params_list),
        total=len(params_list),
        desc="Ejecutando simulaciones",
        unit="sim",
        colour="green"
    ))
    
    # Procesar resultados
    for msg, estado in resultados:
        if estado:
            exitosas += 1
        else:
            if "Timeout" in msg:
                timeouts += 1
            else:
                fallidas += 1
        print(msg)

print(f"\n{'='*50}")
print(f"🎯 SIMULACIONES COMPLETADAS!")
print(f"   • Total: {len(params_list):,}")
print(f"   • ✅ Exitosas: {exitosas:,}")
print(f"   • ❌ Fallidas: {fallidas:,}")
print(f"   • ⏰ Timeouts: {timeouts:,}")
print(f"   • 📁 CSV: {CSV_FILE}")
print(f"   • 📂 Logs: scratch/Estadisticas/{CATEGORY}/Logs/")
print(f"{'='*50}")
