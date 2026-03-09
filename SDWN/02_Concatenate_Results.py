import os
import re
import numpy as np
from tqdm import tqdm
import pandas as pd
from tkinter import Tk, filedialog


def assign_priorities(df, nStaH, nStaM, nStaL):
    """
    Asigna prioridades a los dispositivos según la distribución.
    """    
    priority_list = []
    for i in range(len(df)):
        if i < nStaH:
            priority_list.append("HIGH")
        elif i < nStaH + nStaM:
            priority_list.append("MEDIUM")
        elif i < nStaH + nStaM + nStaL:
            priority_list.append("LOW")
        else:
            priority_list.append("NRT")
    
    return priority_list


# --- Ocultar ventana principal de Tkinter ---
Tk().withdraw()


# --- Subir un nivel desde el directorio actual
directorio_actual = os.getcwd()
directorio_padre = os.path.dirname(directorio_actual)

# --- Seleccionar carpeta raíz ---
root_folder = filedialog.askdirectory(
    initialdir="",
    title="Selecciona la carpeta raíz para buscar archivos CSV"
)

if not root_folder:
    raise Exception("No se seleccionó ninguna carpeta.")


# --- Buscar todos los archivos CSV recursivamente ---
csv_files_info = []

print("🔍 Buscando archivos CSV...")
for root, dirs, files in os.walk(root_folder):
    for file in files:
        if file.endswith(".csv"):
            file_name = file[:-4]
            full_path = os.path.join(root, file)
            csv_files_info.append((file_name, full_path))

print(f"📊 Se encontraron {len(csv_files_info)} archivos CSV.\n")

# --- Inicializar DataFrame vacío ---
full_data = pd.DataFrame()
# --- Inicializar una lista para acumular los DataFrames ---
all_filtered_dfs = []  # Mucho más eficiente que pd.concat en bucle

print("🚀 Iniciando procesamiento de archivos...\n")

for f, file_path in tqdm(csv_files_info, desc="📄 Procesando CSV", unit="archivo"):
    try:
        # 1. Leer el archivo pero ignorar posibles errores de líneas malformadas al final
        data = pd.read_csv(file_path, decimal='.', on_bad_lines='skip')
        data = data.dropna(subset=['nStaWifi'])
        
        if data.empty:
            continue

        # 3. Extraer metadatos del nombre del archivo
        partes = f.split('_')
        RUN = int(partes[-1].split('Run')[1])
        
        # 4. Ahora la conversión a int será segura
        nStaWifi = int(data['nStaWifi'].iloc[0])
        nStaH = int(data['nStaH'].iloc[0])
        nStaM = int(data['nStaM'].iloc[0])
        nStaL = int(data['nStaL'].iloc[0])

        # 5. Aplicar tu lógica de filtrado de IPs
        # Usamos .head(2*nStaWifi) sobre los datos ya limpios
        data_filtered = data.head(nStaWifi).copy()
        

        # 6. Asignar prioridades y metadatos
        data_filtered['Priority'] = assign_priorities(data_filtered, nStaH, nStaM, nStaL)
        data_filtered["RUN"] = RUN
        data_filtered["Source Folder"] = os.path.dirname(file_path)

        all_filtered_dfs.append(data_filtered)
        
    except Exception as e:
        tqdm.write(f"❌ Error procesando {f}.csv: {e}")

# --- Concatenar una sola vez al final ---
if all_filtered_dfs:
    print("\n📦 Consolidando datos...")
    full_data = pd.concat(all_filtered_dfs, ignore_index=True)
    
    output_path = os.path.join(root_folder, "All Data Results.csv")
    full_data.to_csv(output_path, index=False)
    print(f"✅ Resultados guardados en: {output_path}")
    print(f"📊 Total de filas: {len(full_data)}")
else:
    print("❌ No se generaron datos para guardar.")