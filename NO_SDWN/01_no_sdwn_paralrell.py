#!/usr/bin/env python3
import os
import subprocess
import csv
import random
from datetime import datetime
from multiprocessing import Pool, cpu_count

# ================================
# CONFIGURACIÓN BASE
# ================================
PACKET_SIZES = [1024]
TOTAL_DEVICES = [100]
TIME_SIM_MIN = 10
N_CORRIDAS = 10
CATEGORY = "NO-SDWN-TEST" #

# Parámetros SDWN específicos
MOBILITY_TYPE = "mixer"  # "yes", "no", o "mixer"
ENABLE_PCAP = False

# Configuración de dispositivos por prioridad
CONFIGURACION_DEVICES = {
    # 10: (3, 3, 2, 2), 20: (5, 5, 5, 5), 30: (8, 8, 7, 7), 40: (10, 10, 10, 10), 50: (13, 13, 12, 12), 60: (15, 15, 15, 15), 70: (18, 18, 17, 17), 80: (20, 20, 20, 20), 90: (23, 23, 22, 22),
    100: (40, 30, 20, 10)
}

# Parámetros CW (valores por defecto)
CW_PARAMS = {
    'H':    (15, 1023),     # High (VO)
    'M':    (15, 1023),     # Medium (VI)
    'L':    (15, 1023),     # Low (BE)
    'NRT':  (15, 1023)      # Non-Real Time (BK)
}

# ================================
# CREAR DIRECTORIOS
# ================================
os.makedirs(f"scratch/Estadisticas/{CATEGORY}/Logs", exist_ok=True)
os.makedirs(f"scratch/Estadisticas/{CATEGORY}/Results_Finals/", exist_ok=True)
CSV_FILE = f"scratch/Estadisticas/{CATEGORY}/Results_Finals/{CATEGORY}_Summary.csv"

# ================================
# FUNCIÓN PARA EJECUTAR UNA SIMULACIÓN
# ================================
def run_simulation(params):
    sim_id, packet_size, total, corrida, seed = params
    
    # Obtener configuración de dispositivos para este total
    nStaH, nStaM, nStaL, nStaNRT = CONFIGURACION_DEVICES[total]
    nStaWifi = nStaH + nStaM + nStaL + nStaNRT
    
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    log_name = f"{CATEGORY}_T{total}_H{nStaH}M{nStaM}L{nStaL}N{nStaNRT}_PS{packet_size}_Run{corrida}_S{seed}_{timestamp}.log"
    log_path = os.path.join(f"scratch/Estadisticas/{CATEGORY}/Logs", log_name)

    # 1. Usamos la ruta directa al binario compilado
    BIN_PATH = "./build/scratch/ns3.46.1-no_sdwn-default"

    # Construir comando con todos los parámetros SDWN
    cmd = (
        f'{BIN_PATH} '
        f'--nStaWifi={nStaWifi} --nStaH={nStaH} --nStaM={nStaM} --nStaL={nStaL} --nStaNRT={nStaNRT} '
        f'--PacketSize={packet_size} '
        f'--TimeSimulationMin={TIME_SIM_MIN} '
        f'--nCorrida={corrida} '
        f'--RngSeed={seed} '
        f'--category={CATEGORY} '
        f'--mobilityType={MOBILITY_TYPE} '
        f'--enablePcap={"true" if ENABLE_PCAP else "false"}'
    )

    with open(log_path, "w") as log_file:
        result = subprocess.run(cmd, shell=True, stdout=log_file, stderr=subprocess.STDOUT)

    if result.returncode != 0:
        return f"[{sim_id}] ❌ Error en simulación (ver log: {log_name})"
    else:
        return f"[{sim_id}] ✅ Completada"

# ================================
# GENERAR TODAS LAS SIMULACIONES Y CSV
# ================================
sim_id = 0
params_list = []


# Crear CSV de resumen
with open(CSV_FILE, "w", newline="") as csvfile:
    writer = csv.writer(csvfile)
    header = [
        "SimID", "PacketSize", "TotalDevices", "nStaH", "nStaM", "nStaL", "nStaNRT",
        "CwMinH", "CwMaxH", "CwMinM", "CwMaxM", "CwMinL", "CwMaxL", 
        "CwMinNRT", "CwMaxNRT", "Run", "Seed", "Category", "MobilityType"
    ]
    writer.writerow(header)

    idx_semilla = 0
    for total in TOTAL_DEVICES:
        for packet_size in PACKET_SIZES:
        
            # Obtener configuración para este total
            nStaH, nStaM, nStaL, nStaNRT = CONFIGURACION_DEVICES[total]
            
            for corrida in range(1, N_CORRIDAS + 1):
                sim_id += 1
                seed = corrida
                
                # Agregar a lista de parámetros
                params_list.append((sim_id, packet_size, total, corrida, seed))
                
                # Escribir en CSV
                writer.writerow([
                    sim_id, packet_size, total, nStaH, nStaM, nStaL, nStaNRT,
                    CW_PARAMS["H"][0], CW_PARAMS["H"][1],
                    CW_PARAMS["M"][0], CW_PARAMS["M"][1],
                    CW_PARAMS["L"][0], CW_PARAMS["L"][1],
                    CW_PARAMS["NRT"][0], CW_PARAMS["NRT"][1],
                    corrida, seed, CATEGORY, MOBILITY_TYPE
                ])

# ================================
# EJECUTAR EN PARALELO
# ================================
num_cpus = min(64, cpu_count() - 2)
print(f"\n💻 Ejecutando simulaciones en paralelo con {num_cpus} núcleos...\n")

with Pool(processes=num_cpus) as pool:
    for i, result_msg in enumerate(pool.imap_unordered(run_simulation, params_list), 1):
        print(result_msg)
        if i % 10 == 0:
            print(f"📊 Progreso: {i}/{sim_id} ({i/sim_id*100:.1f}%)")

print(f"\n🎯 Todas las {sim_id} simulaciones completadas.")