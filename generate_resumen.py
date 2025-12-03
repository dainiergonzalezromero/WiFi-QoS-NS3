import os
import re
import pandas as pd
from tkinter import Tk, filedialog

# --- Ocultar ventana principal de Tkinter ---
Tk().withdraw()

# --- Seleccionar archivo inicial ---
file_path = filedialog.askopenfilename(
    title="Selecciona un archivo CSV",
    filetypes=[("CSV Files", "*.csv")]
)

if not file_path:
    raise Exception("No se seleccionó ningún archivo.")

# --- Carpeta donde están los CSV ---
folder_path = os.path.dirname(file_path)

# --- Listar archivos CSV en la carpeta ---
csv_files = [f.split(".")[0] for f in os.listdir(folder_path) if f.endswith(".csv")]
print(f"Se encontraron {len(csv_files)} archivos CSV.")

# --- Agrupar por experimento (quitando el sufijo _RunX) ---
file_groups = {}
for f in csv_files:
    base_name = re.sub(r"_Run\d+$", "", f)
    file_groups.setdefault(base_name, []).append(f)

TOTAL_DEVICES = (f.split(sep='_')[2][:-3])
print(TOTAL_DEVICES)
PACKET_SIZE = int(f.split(sep='_')[3][:-1])

# --- Cargar el archivo de simulaciones ---
path_simulations = os.path.join(folder_path, "Results_Finals")
file_simulations = os.path.join(path_simulations, f"Simulations_Summary_[{TOTAL_DEVICES}]_[{PACKET_SIZE}B].csv")

# Leer el CSV (ajusta el separador si no es coma)
simulations = pd.read_csv(file_simulations, sep=",")

# --- 1️⃣ Eliminar columnas no necesarias ---
simulations = simulations.drop(columns=["SimID", "Run", "DistID"], errors="ignore")

# --- 2️⃣ Eliminar filas duplicadas ---
simulations = simulations.drop_duplicates()

print(f"Simulaciones cargadas: {len(simulations)} filas únicas")

simulations.head()

# --- Expresión regular para extraer CWMin y CWMax ---
pattern = re.compile(
    r"CWMin\((\d+)-(\d+)-(\d+)-(\d+)\)_CWMax\((\d+)-(\d+)-(\d+)-(\d+)\)"
)

# --- Crear carpeta "Updated" para guardar los nuevos CSV ---
updated_folder = os.path.join(folder_path, "Updated")
os.makedirs(updated_folder, exist_ok=True)

# --- Iterar sobre los grupos de archivos ---
for base_name, files in file_groups.items():
    match = pattern.search(base_name)
    if not match:
        print(f"⚠️ No se pudo extraer parámetros de {base_name}")
        continue

    # Extraer los valores numéricos
    CwMinH, CwMinM, CwMinL, CwMinNRT, CwMaxH, CwMaxM, CwMaxL, CwMaxNRT = map(int, match.groups())

    # Buscar coincidencia exacta en simulations
    match_row = simulations[
        (simulations["CwMinH"] == CwMinH) &
        (simulations["CwMinM"] == CwMinM) &
        (simulations["CwMinL"] == CwMinL) &
        (simulations["CwMinNRT"] == CwMinNRT) &
        (simulations["CwMaxH"] == CwMaxH) &
        (simulations["CwMaxM"] == CwMaxM) &
        (simulations["CwMaxL"] == CwMaxL) &
        (simulations["CwMaxNRT"] == CwMaxNRT)
    ]

    if match_row.empty:
        print(f"❌ No se encontró coincidencia en simulations para {base_name}")
        continue

    # Tomar los valores que se agregarán
    row = match_row.iloc[0]
    extra_data = {
        "TotalDevices": row["TotalDevices"],
        "nStaH": row["nStaH"],
        "nStaM": row["nStaM"],
        "nStaL": row["nStaL"],
        "nStaNRT": row["nStaNRT"]
    }

    # --- Agregar los valores a cada CSV del grupo ---
    for f in files:
        csv_path = os.path.join(folder_path, f"{f}.csv")

        if not os.path.exists(csv_path):
            print(f"⚠️ Archivo no encontrado: {csv_path}")
            continue

        # Leer el CSV protegiendo contra archivos vacíos o mal formados
        try:
            df = pd.read_csv(csv_path)
        except pd.errors.EmptyDataError:
            print(f"⚠️ Archivo vacío, se omite: {csv_path}")
            continue
        except Exception as e:
            print(f"⚠️ Error leyendo {csv_path}: {e}")
            continue

        # Quitar columnas "Unnamed" solo si existen columnas
        if df.shape[1] == 0 or df.empty:
            print(f"⚠️ El archivo no contiene columnas válidas o está vacío, se omite: {csv_path}")
            continue

        df = df.loc[:, ~df.columns.str.contains("^Unnamed")]
        df = df.reset_index(drop=True)  # asegurar índices 0..n-1

        # --- Agregar los valores solo a las primeras N filas (TotalDevices) ---
        n_devices = int(extra_data["TotalDevices"])
        n_devices = min(n_devices, len(df))  # Ajustar si hay menos filas que TotalDevices

        for col, val in extra_data.items():
            if col not in df.columns:
                df[col] = None
            # asignar solo a los índices existentes
            if n_devices > 0:
                df.loc[:n_devices - 1, col] = val

        # --- Agregar columna Prioridad ---
        nH = int(extra_data["nStaH"])
        nM = int(extra_data["nStaM"])
        nL = int(extra_data["nStaL"])
        nNRT = int(extra_data["nStaNRT"])

        df["Prioridad"] = None
        # proteger rangos para no exceder el tamaño del DataFrame
        end_h = min(nH, len(df))
        end_m = min(nH + nM, len(df))
        end_l = min(nH + nM + nL, len(df))
        end_nrt = min(nH + nM + nL + nNRT, len(df))

        if end_h > 0:
            df.loc[0:end_h-1, "Prioridad"] = "HIGH"
        if end_m > nH:
            df.loc[nH:end_m-1, "Prioridad"] = "MEDIUM"
        if end_l > (nH + nM):
            df.loc[nH+nM:end_l-1, "Prioridad"] = "LOW"
        if end_nrt > (nH + nM + nL):
            df.loc[nH+nM+nL:end_nrt-1, "Prioridad"] = "NRT"

        # Guardar en la carpeta "Updated"
        output_path = os.path.join(updated_folder, f"{f}.csv")
        try:
            df.to_csv(output_path, index=False)
            print(f"✅ Archivo actualizado guardado en: {output_path}")
        except Exception as e:
            print(f"⚠️ Error guardando {output_path}: {e}")

print("\nProceso completado ✅ Todos los archivos actualizados están en la carpeta 'Updated'.")

# Carpeta donde están los archivos actualizados
updated_folder = os.path.join(folder_path, "Updated")

all_dfs = []

# Columnas que son enteros
int_cols = [
    "LostPackets", "SentPackets", "ReceivedPackets",
    "CWminH","CWmaxH","CWminM","CWmaxM","CWminL","CWmaxL","CWminNRT","CWmaxNRT",
    "TotalDevices","nStaH","nStaM","nStaL","nStaNRT"
]

# Columnas que son floats
float_cols = ["Throughput(Kbps)", "Delay(ms)"]

# Leer solo las primeras n_devices de cada archivo
for f in os.listdir(updated_folder):
    if f.endswith(".csv"):
        csv_path = os.path.join(updated_folder, f)
        df = pd.read_csv(csv_path)
        # Determinar cuántas filas usar según la columna TotalDevices
        n_devices = int(df.loc[0, "TotalDevices"])
        n_devices = min(n_devices, len(df))  # No exceder filas disponibles
        df = df.iloc[:n_devices, :]  # Tomar solo las primeras n_devices filas

        # Convertir columnas enteras
        for col in int_cols:
            if col in df.columns:
                df[col] = df[col].fillna(0).astype(int)  # NaN a 0 y entero

        # Convertir columnas float
        for col in float_cols:
            if col in df.columns:
                df[col] = df[col].fillna(0).astype(float)

        all_dfs.append(df)

# Concatenar todos los DataFrames
final_df = pd.concat(all_dfs, ignore_index=True)

# Guardar en CSV
final_output_path = os.path.join(folder_path, "01_Todos_Concatenados.csv")
final_df.to_csv(final_output_path, index=False, float_format="%.2f")  # 2 decimales para floats

print(f"✅ Todos los archivos concatenados en: {final_output_path}")

file_final = os.path.join(folder_path, "01_Todos_Concatenados.csv")

# Leer CSV original
df = pd.read_csv(file_final, decimal=",")  # decimal=',' para floats

# Asegurar tipos
float_cols = ["Throughput(Kbps)", "Delay(ms)"]
int_cols = [
    "LostPackets", "SentPackets", "ReceivedPackets",
    "CWminH","CWmaxH","CWminM","CWmaxM","CWminL","CWmaxL","CWminNRT","CWmaxNRT",
    "TotalDevices","nStaH","nStaM","nStaL","nStaNRT"
]

for col in float_cols:
    if col in df.columns:
        df[col] = df[col].astype(float)
for col in int_cols:
    if col in df.columns:
        df[col] = df[col].astype(int)

# Columnas constantes por grupo
group_cols = [
    "CWminH","CWmaxH","CWminM","CWmaxM","CWminL","CWmaxL","CWminNRT","CWmaxNRT",
    "TotalDevices","nStaH","nStaM","nStaL","nStaNRT"
]

# Función auxiliar para calcular agregados por prioridad
def agg_priority(subdf):
    res = {}
    for priority, name in zip(["HIGH","MEDIUM","LOW","NRT"], ["H","M","L","noRT"]):
        prio_df = subdf[subdf["Prioridad"]==priority]
        if len(prio_df)==0:
            res[f"Throughput_{name}"] = 0
            res[f"Delay_{name}"] = 0
            res[f"Loss_Packets_{name}"] = 0
        else:
            res[f"Throughput_{name}"] = prio_df["Throughput(Kbps)"].mean()
            res[f"Delay_{name}"] = prio_df["Delay(ms)"].mean()
            total_lost = prio_df["LostPackets"].sum()
            total_sent = prio_df["SentPackets"].sum()
            res[f"Loss_Packets_{name}"] = (total_lost / total_sent * 100) if total_sent > 0 else 0
    return pd.Series(res)

# Agrupar y agregar
agg_df = df.groupby(group_cols).apply(agg_priority).reset_index()

# Renombrar columnas según tu mapeo
agg_df = agg_df.rename(columns={
    "TotalDevices":"Total_Devices",
    "nStaH":"Nodes_High",
    "nStaM":"Nodes_Medium",
    "nStaL":"Nodes_Low",
    "nStaNRT":"Nodes_NoRT",
    "CWminH":"CwMinH",
    "CWmaxH":"CwMaxH",
    "CWminM":"CwMinM",
    "CWmaxM":"CwMaxM",
    "CWminL":"CwMinL",
    "CWmaxL":"CwMaxL",
    "CWminNRT":"CwMinNoRT",
    "CWmaxNRT":"CwMaxNoRT"
})

# Reordenar columnas: primero constantes, luego agregadas por prioridad
fixed_cols = [
    "Total_Devices",
    "Nodes_High",
    "Nodes_Medium",
    "Nodes_Low",
    "Nodes_NoRT",
    "CwMinH",
    "CwMaxH",
    "CwMinM",
    "CwMaxM",
    "CwMinL",
    "CwMaxL",
    "CwMinNoRT",
    "CwMaxNoRT"
]

priority_suffixes = ["H","M","L","noRT"]
calculated_cols = []
for suf in priority_suffixes:
    calculated_cols += [f"Throughput_{suf}", f"Delay_{suf}", f"Loss_Packets_{suf}"]

agg_df = agg_df[fixed_cols + calculated_cols]

# Guardar CSV final
final_output_path = os.path.join(folder_path, "02_Resumen_Por_Prioridad.csv")
agg_df.to_csv(final_output_path, index=False, float_format="%.2f", sep=";")

print("✅ Archivo agregado generado: 02_Resumen_Por_Prioridad.csv con columnas en orden fijo")

