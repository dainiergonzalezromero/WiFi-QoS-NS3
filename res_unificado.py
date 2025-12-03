import os
from tkinter import Tk, filedialog
from pathlib import Path
import pandas as pd

# Oculta la ventana principal de Tkinter
Tk().withdraw()

# Selecciona la carpeta
ruta_base = filedialog.askdirectory(title="Selecciona la carpeta donde buscar")
ruta_base = Path(ruta_base)

print(f"Carpeta seleccionada:\n{ruta_base}\n")

# Busca los archivos dentro de la carpeta y subcarpetas
archivos = list(ruta_base.rglob("*02_Resumen_Por_Prioridad.csv"))

if not archivos:
    print("No se encontraron archivos.")
    exit()

filas_finales = []
columnas_base = None  # Para guardar el orden original

for archivo in archivos:
    # Lee archivo csv
    df = pd.read_csv(archivo, sep=";")

    # Guardamos el orden original de columnas SOLO del primer archivo
    if columnas_base is None:
        columnas_base = list(df.columns)

    # Se asume una sola fila por archivo
    fila = df.iloc[0].copy()

    # Obtención robusta del número de Packet_Size desde la carpeta donde está el archivo
    Packet_Size = int(str(archivo).split("/")[-2])

    # Agregamos la columna Packet_Size
    fila["Packet_Size"] = Packet_Size

    filas_finales.append(fila)

# Crear DataFrame final
df_final = pd.DataFrame(filas_finales)

# Insertar Packet_Size después de Total_Devices
if "Total_Devices" not in columnas_base:
    raise ValueError("La columna 'Total_Devices' no existe en los archivos CSV.")

# Construimos el orden final:
cols_final = []
for col in columnas_base:
    cols_final.append(col)
    if col == "Total_Devices":
        cols_final.append("Packet_Size")  # insertar justo después

df_final = df_final[cols_final]

# Ordenar por Total_Devices y Packet_Size
df_final = df_final.sort_values(by=["Packet_Size", "Total_Devices" ], ascending=[True, True])

# Exportar CSV final
output_path = ruta_base / "Resumen_Unificado.csv"
df_final.to_csv(output_path, sep=";", index=False)

print(f"\nArchivo combinado creado y ordenado:\n{output_path}")
