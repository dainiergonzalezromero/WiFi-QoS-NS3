#!/usr/bin/env python3
import os
import csv
import subprocess
from datetime import datetime
from multiprocessing import Pool, cpu_count

# ================================
# CONFIGURACIÓN BASE
# ================================
NS3_PATH = "./ns3"          # Ejecutable NS-3 relativo a la raíz
SCRIPT = "scratch/sdwn.cc"  # Ruta relativa al script desde la raíz de NS-3

# ================================
# PARÁMETROS DE LA SIMULACIÓN
# ================================
PACKET_SIZES = [256]  # Lista de tamaños de paquete
TIME_SIM_MIN = 10
N_CORRIDAS = 20  # Número de repeticiones por conjunto de parámetros

TOTAL_DEVICES = 60
nStaH = 15
nStaM = 15
nStaL = 15
nStaNRT = 15

cw_params = {
    'H': (255, 1023),
    'M': (63, 1023),
    'L': (31, 255),
    'noRT': (7, 255),
}

cwH = cw_params['H']
cwM = cw_params['M']
cwL = cw_params['L']
cwNRT = cw_params['noRT']

# ================================
# RUTAS DE LOGS
# ================================
RESULTS_DIR = os.path.join("scratch", "Estadisticas", "Logs")
os.makedirs(RESULTS_DIR, exist_ok=True)

# ================================
# FUNCIÓN PARA EJECUTAR UNA SIMULACIÓN
# ================================
def run_simulation(params):
    sim_id, total, dist_id, nStaH, nStaM, nStaL, nStaNRT, \
    cwH, cwM, cwL, cwNRT, corrida, packet_size, results_csv_path = params

    CwMinH, CwMaxH = cwH
    CwMinM, CwMaxM = cwM
    CwMinL, CwMaxL = cwL
    CwMinNRT, CwMaxNRT = cwNRT

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    log_name = (
        f"Run{corrida}_Pkt{packet_size}_H{nStaH}M{nStaM}L{nStaL}NRT{nStaNRT}_"
        f"cwH{CwMinH}-{CwMaxH}_cwM{CwMinM}-{CwMaxM}_"
        f"cwL{CwMinL}-{CwMaxL}_cwNRT{CwMinNRT}-{CwMaxNRT}_"
        f"{timestamp}.log"
    )
    log_path = os.path.join(RESULTS_DIR, log_name)

    script_with_args = (
        f"{SCRIPT} "
        f"--nStaH={nStaH} --nStaM={nStaM} --nStaL={nStaL} --nStaNRT={nStaNRT} "
        f"--CwMinH={CwMinH} --CwMaxH={CwMaxH} "
        f"--CwMinM={CwMinM} --CwMaxM={CwMaxM} "
        f"--CwMinL={CwMinL} --CwMaxL={CwMaxL} "
        f"--CwMinNRT={CwMinNRT} --CwMaxNRT={CwMaxNRT} "
        f"--PacketSize={packet_size} --TimeSimulationMin={TIME_SIM_MIN} --nCorrida={corrida}"
    )

    cmd = f'{NS3_PATH} run "{script_with_args}"'

    try:
        with open(log_path, "w") as log_file:
            result = subprocess.run(cmd, shell=True, stdout=log_file, stderr=subprocess.STDOUT)

        if result.returncode == 0:
            # Guardar resultado directamente en el CSV
            with open(results_csv_path, "a", newline="") as csvfile:
                writer = csv.writer(csvfile)
                writer.writerow([
                    sim_id, total, dist_id, nStaH, nStaM, nStaL, nStaNRT,
                    CwMinH, CwMaxH, CwMinM, CwMaxM, CwMinL, CwMaxL,
                    CwMinNRT, CwMaxNRT, packet_size, corrida
                ])
            return (True, sim_id, packet_size, corrida)
        else:
            return (False, sim_id, f"Return code {result.returncode}")
    except Exception as e:
        return (False, sim_id, str(e))

# ================================
# FUNCIÓN PRINCIPAL
# ================================
def main():
    global nStaH, nStaM, nStaL, nStaNRT, cwH, cwM, cwL, cwNRT

    total_nodes = nStaH + nStaM + nStaL + nStaNRT
    print(f"\n🔹 Total de nodos: {total_nodes}")
    print(f"🔹 Ejecutando {N_CORRIDAS} corridas por cada tamaño de paquete\n")

    sim_id = 0
    dist_id = 1
    num_cpus = max(1, cpu_count()-2)

    print(f"💻 Ejecutando simulaciones en paralelo con {num_cpus} núcleos...\n")

    # Bucle por cada tamaño de paquete
    for pkt_size in PACKET_SIZES:
        print(f"📦 Iniciando simulaciones para Packet Size = {pkt_size} bytes\n")

        # === Crear carpeta específica por tamaño de paquete ===
        FINAL_CSV_DIR = os.path.join(
            "scratch", "Estadisticas", "BE+BK+VI+VO", "1S", "Modified",
            f"{TOTAL_DEVICES}",f"{pkt_size}", "Results_Finals"
        )
        os.makedirs(FINAL_CSV_DIR, exist_ok=True)

        FINAL_CSV_PATH = os.path.join(FINAL_CSV_DIR, f"Simulations_Summary_[{TOTAL_DEVICES}]_[{pkt_size}B].csv")

        # Crear CSV nuevo con encabezado
        with open(FINAL_CSV_PATH, "w", newline="") as csvfile:
            writer = csv.writer(csvfile)
            header = [
                "SimID", "TotalDevices", "DistID", "nStaH", "nStaM", "nStaL", "nStaNRT",
                "CwMinH", "CwMaxH", "CwMinM", "CwMaxM", "CwMinL", "CwMaxL",
                "CwMinNRT", "CwMaxNRT", "PacketSize", "Run"
            ]
            writer.writerow(header)

        # === Crear lista de parámetros para el Pool ===
        params_list = []
        for corrida in range(1, N_CORRIDAS + 1):
            sim_id += 1
            params_list.append((
                sim_id, TOTAL_DEVICES, dist_id,
                nStaH, nStaM, nStaL, nStaNRT,
                cwH, cwM, cwL, cwNRT, corrida, pkt_size, FINAL_CSV_PATH
            ))

        # === Ejecutar simulaciones en paralelo ===
        with Pool(processes=num_cpus) as pool:
            for result in pool.imap_unordered(run_simulation, params_list):
                if result[0]:
                    _, sim_id_done, packet_size, corrida = result
                    print(f"[Packet Size {packet_size} | Run {corrida}] ✅ Completada")
                else:
                    print(f"[SimID {result[1]}] ❌ Error: {result[2]}")

    print("\n🎯 Todas las simulaciones completadas correctamente.")
    print("📂 Resultados guardados en carpetas separadas por tamaño de paquete.")

# ================================
# ENTRADA PRINCIPAL
# ================================
if __name__ == "__main__":
    main()
