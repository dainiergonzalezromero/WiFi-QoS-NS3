#!/usr/bin/env python3
import os
import subprocess
import csv
import random
from datetime import datetime
from multiprocessing import Pool, cpu_count
from tqdm import tqdm

# ================================
# CONFIGURACIÓN BASE
# ================================
CATEGORY = "SDWN-IA-TEST"
TIME_SIM_MIN = 10
N_CORRIDAS = 10
MOBILITY_TYPE = "mixer"
ENABLE_PCAP = False

# Archivo CSV con las configuraciones óptimas
OPTIMAL_CONFIGS_FILE = "scratch/Resultados_Optimizacion_IA_Test/Optimizacion_CW_Final.csv"

# Fijar semilla base
BASE_SEED = 42
random.seed(BASE_SEED)

# ================================
# CREAR DIRECTORIOS
# ================================
os.makedirs(f"scratch/Estadisticas/{CATEGORY}/Results_Finals/", exist_ok=True)
CSV_FILE = f"scratch/Estadisticas/{CATEGORY}/Results_Finals/{CATEGORY}_Summary.csv"

# ================================
# FUNCIÓN PARA CARGAR CONFIGURACIONES ÓPTIMAS
# ================================
def cargar_configuraciones_optimas():
    """Carga las configuraciones óptimas de CW desde el CSV"""
    configs = []
    
    if not os.path.exists(OPTIMAL_CONFIGS_FILE):
        print(f"⚠️  No se encuentra el archivo: {OPTIMAL_CONFIGS_FILE}")
        print("Ejecuta primero el script de optimización para generar las configuraciones.")
        return configs
    
    with open(OPTIMAL_CONFIGS_FILE, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            config = {
                'Packet_Size': int(row['Packet Size']),
                'nStaWifi': int(row['nStaWifi']),
                'nStaH': int(row['nStaH']),
                'nStaM': int(row['nStaM']),
                'nStaL': int(row['nStaL']),
                'nStaNRT': int(row['nStaNRT']),
                'CWminH': int(row['CWminH']),
                'CWmaxH': int(row['CWmaxH']),
                'CWminM': int(row['CWminM']),
                'CWmaxM': int(row['CWmaxM']),
                'CWminL': int(row['CWminL']),
                'CWmaxL': int(row['CWmaxL']),
                'CWminNRT': int(row['CWminNRT']),
                'CWmaxNRT': int(row['CWmaxNRT']),
            }
            configs.append(config)
    
    print(f"✅ Cargadas {len(configs)} configuraciones óptimas desde {OPTIMAL_CONFIGS_FILE}")
    return configs

# ================================
# FUNCIÓN PARA EJECUTAR UNA SIMULACIÓN
# ================================
def run_simulation(params):
    sim_id, config, corrida, seed = params

    # Ruta al binario compilado
    BIN_PATH = "./build/scratch/ns3.46.1-sdwn-default"

    # Construir comando con las configuraciones óptimas
    cmd = (
        f'{BIN_PATH} '
        f'--nStaWifi={config["nStaWifi"]} --nStaH={config["nStaH"]} --nStaM={config["nStaM"]} --nStaL={config["nStaL"]} --nStaNRT={config["nStaNRT"]} '
        f'--CwMinH={config["CWminH"]} --CwMaxH={config["CWmaxH"]} '
        f'--CwMinM={config["CWminM"]} --CwMaxM={config["CWmaxM"]} '
        f'--CwMinL={config["CWminL"]} --CwMaxL={config["CWmaxL"]} '
        f'--CwMinNRT={config["CWminNRT"]} --CwMaxNRT={config["CWmaxNRT"]} '
        f'--PacketSize={config["Packet_Size"]} '
        f'--TimeSimulationMin={TIME_SIM_MIN} '
        f'--nCorrida={corrida} '
        f'--RngSeed={seed} '
        f'--category={CATEGORY} '
        f'--mobilityType={MOBILITY_TYPE} '
        f'--enablePcap={"true" if ENABLE_PCAP else "false"}'
    )

    try:
        # Redirigir salida a /dev/null para no guardar logs
        with open(os.devnull, 'w') as devnull:
            result = subprocess.run(cmd, shell=True, stdout=devnull, stderr=devnull, timeout=3600)
        
        if result.returncode != 0:
            return {
                'sim_id': sim_id,
                'status': 'error',
                'message': f"Error (código {result.returncode})"
            }
        else:
            return {
                'sim_id': sim_id,
                'status': 'success',
                'message': f"Completada"
            }
    except subprocess.TimeoutExpired:
        return {
            'sim_id': sim_id,
            'status': 'timeout',
            'message': f"Timeout (excedió 1 hora)"
        }
    except Exception as e:
        return {
            'sim_id': sim_id,
            'status': 'error',
            'message': f"Excepción: {str(e)}"
        }

# ================================
# GENERAR TODAS LAS SIMULACIONES Y CSV
# ================================
def main():
    # Cargar configuraciones óptimas
    configuraciones = cargar_configuraciones_optimas()
    
    if not configuraciones:
        print("❌ No hay configuraciones para simular. Saliendo...")
        return
    
    sim_id = 0
    params_list = []
    
    # Crear CSV de resumen
    with open(CSV_FILE, "w", newline="") as csvfile:
        writer = csv.writer(csvfile)
        header = [
            "SimID", "PacketSize", "TotalDevices", "nStaH", "nStaM", "nStaL", "nStaNRT",
            "CwMinH", "CwMaxH", "CwMinM", "CwMaxM", "CwMinL", "CwMaxL", 
            "CwMinNRT", "CwMaxNRT", "Run", "Seed", "Category", "MobilityType", "Status"
        ]
        writer.writerow(header)
        
        # Para cada configuración óptima, ejecutar N_CORRIDAS simulaciones
        for config in configuraciones:
            for corrida in range(1, N_CORRIDAS + 1):
                sim_id += 1
                # Generar semilla aleatoria única
                seed = N_CORRIDAS
                
                # Agregar a lista de parámetros
                params_list.append((sim_id, config, corrida, seed))
                
                # Escribir en CSV (inicialmente con Status pendiente)
                writer.writerow([
                    sim_id, 
                    config['Packet_Size'], 
                    config['nStaWifi'], 
                    config['nStaH'], 
                    config['nStaM'], 
                    config['nStaL'], 
                    config['nStaNRT'],
                    config['CWminH'], 
                    config['CWmaxH'],
                    config['CWminM'], 
                    config['CWmaxM'],
                    config['CWminL'], 
                    config['CWmaxL'],
                    config['CWminNRT'], 
                    config['CWmaxNRT'],
                    corrida, 
                    seed, 
                    CATEGORY, 
                    MOBILITY_TYPE,
                    "PENDING"
                ])
    
    print(f"\n📊 Plan de simulaciones:")
    print(f"   - {len(configuraciones)} configuraciones óptimas")
    print(f"   - {N_CORRIDAS} repeticiones por configuración")
    print(f"   - Total: {sim_id} simulaciones")
    print(f"   - CSV generado: {CSV_FILE}")
    
    # ================================
    # EJECUTAR EN PARALELO CON TQDM
    # ================================
    num_cpus = min(64, cpu_count() - 2)
    print(f"\n💻 Ejecutando simulaciones en paralelo con {num_cpus} núcleos...\n")
    
    # Estadísticas para el resumen final
    stats = {
        'success': 0,
        'error': 0,
        'timeout': 0
    }
    
    # Usar Pool con tqdm para barra de progreso
    with Pool(processes=num_cpus) as pool:
        resultados = list(tqdm(
            pool.imap_unordered(run_simulation, params_list),
            total=len(params_list),
            desc="🚀 Simulando",
            unit="sim",
            ncols=100,
            colour="green"
        ))
    
    # Procesar resultados y actualizar CSV
    print("\n\n📝 Procesando resultados...")
    
    # Leer el CSV existente
    rows = []
    with open(CSV_FILE, 'r') as f:
        reader = csv.reader(f)
        header = next(reader)
        rows = list(reader)
    
    # Actualizar estados según resultados
    for resultado in resultados:
        sim_id = resultado['sim_id']
        stats[resultado['status']] += 1
        
        # Actualizar el estado en rows
        for row in rows:
            if int(row[0]) == sim_id:
                row[-1] = resultado['status'].upper()
                break
        
        # Mostrar errores si los hay (usando tqdm.write para no interferir con la barra)
        if resultado['status'] != 'success':
            tqdm.write(f"⚠️  Simulación {sim_id}: {resultado['message']}")
    
    # Guardar CSV actualizado
    with open(CSV_FILE, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(header)
        writer.writerows(rows)
    

# ================================
# EJECUTAR FUNCIÓN PRINCIPAL
# ================================
if __name__ == "__main__":
    main()