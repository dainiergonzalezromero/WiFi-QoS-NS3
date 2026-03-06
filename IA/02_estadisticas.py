import os
import numpy as np
import pandas as pd
from tqdm import tqdm
from tkinter import Tk, filedialog
import openpyxl
from openpyxl.utils import get_column_letter

# --- Ocultar ventana principal de Tkinter ---
Tk().withdraw()

# --- Configuración de rutas ---
directorio_actual = os.getcwd()
directorio_padre = os.path.dirname(directorio_actual)

# --- Seleccionar archivo consolidado (All_Data_Results.csv) ---
file_path = filedialog.askopenfilename(
    initialdir=directorio_padre, 
    title="Selecciona el archivo All_Data_Results.csv",
    filetypes=[("CSV Files", "*.csv")]
)

if not file_path:
    raise Exception("No se seleccionó ningún archivo.")

# ==============================================
# CONFIGURACIÓN DE ANÁLISIS ESTADÍSTICO EXTENDIDO
# ==============================================

print(f"\n📂 Leyendo archivo: {os.path.basename(file_path)}")
data = pd.read_csv(file_path, decimal='.')

# -- Columnas de entrada (Features) para agrupar
GroupBy_Columns = [
    'Packet Size', 'nStaWifi', 'nStaH', 'nStaM', 'nStaL', 'nStaNRT',
    'CWminH', 'CWmaxH', 'CWminM', 'CWmaxM', 'CWminL', 'CWmaxL', 'CWminNRT', 'CWmaxNRT'
]

Priorities = ['HIGH', 'MEDIUM', 'LOW', 'NRT']
priority_suffix = {'HIGH': 'H', 'MEDIUM': 'M', 'LOW': 'L', 'NRT': 'NRT'}

# Agrupar datos por configuración de simulación
data_grouped = data.groupby(GroupBy_Columns)
grupos = list(data_grouped)

# Lista para almacenar los resultados procesados
resultados = []

# Barra de progreso para el procesamiento de grupos
for group_values, group_df in tqdm(grupos, desc="📊 Calculando métricas", unit="grupo"):
    config_dict = dict(zip(GroupBy_Columns, group_values))
    result_row = config_dict.copy()
    
    for priority in Priorities:
        sfx = priority_suffix[priority]
        n_stations = config_dict.get(f'nSta{sfx}', 0)
        
        # SI NO HAY ESTACIONES: Forzar ceros en todas las métricas de la categoría
        if n_stations == 0:
            for metric in ['Throughput', 'Delay']:
                for stat in ['_mean', '_std', '_min', '_max']:
                    result_row[f'{metric}_{sfx}{stat}'] = 0.0
            result_row[f'LostPackets_{sfx}_mean'] = 0.0
            result_row[f'LostPackets_{sfx}_sum'] = 0
            result_row[f'SendPackets_{sfx}_sum'] = 0
            continue

        # FILTRADO DE DATOS REALES
        priority_df = group_df[group_df['Priority'] == priority].copy()
        p_data = priority_df[priority_df['LostPackets'] < 0.99*180].copy()
        
        if not p_data.empty:
            # Aplicar penalización de Timeout al Delay
            p_data['Delay(ms)'] = (p_data['Delay(ms)']*p_data['ReceivedPackets'] + 10 * p_data['LostPackets'])/p_data['SentPackets']

            # --- METRICAS DE THROUGHPUT ---
            result_row[f'Throughput_{sfx}_mean'] = round(p_data['Throughput(Kbps)'].mean(), 5)
            result_row[f'Throughput_{sfx}_std'] = round(p_data['Throughput(Kbps)'].std(), 5)
            result_row[f'Throughput_{sfx}_min'] = round(p_data['Throughput(Kbps)'].min(), 5)
            result_row[f'Throughput_{sfx}_max'] = round(p_data['Throughput(Kbps)'].max(), 5)
            
            # --- METRICAS DE DELAY ---
            result_row[f'Delay_{sfx}_mean'] = round(p_data['Delay(ms)'].mean(), 5)
            result_row[f'Delay_{sfx}_std'] = round(p_data['Delay(ms)'].std(), 5)
            result_row[f'Delay_{sfx}_min'] = round(p_data['Delay(ms)'].min(), 5)
            result_row[f'Delay_{sfx}_max'] = round(p_data['Delay(ms)'].max(), 5)
            
            # --- METRICAS DE PÉRDIDA ---
            total_sent = p_data['SentPackets'].sum()
            total_lost = p_data['LostPackets'].sum()
            result_row[f'LostPackets_{sfx}_mean'] = round((total_lost / total_sent * 100), 5) if total_sent > 0 else 0.0
            result_row[f'LostPackets_{sfx}_sum'] = total_lost
            result_row[f'SendPackets_{sfx}_sum'] = total_sent

    resultados.append(result_row)


# Crear DataFrame final
resultados_df = pd.DataFrame(resultados).fillna(0.0)

# --- GUARDAR RESULTADOS ---

# Guardar CSV
csv_output = os.path.join(os.path.dirname(file_path), 'Resultados_Finales.csv')
resultados_df.to_csv(csv_output, index=False)
print(f"\n📄 Archivo CSV guardado en: {csv_output}")

# Guardar Excel con formato
excel_output = os.path.join(os.path.dirname(file_path), 'Resultados_Finales.xlsx')
print(f"💾 Guardando resultados en formato Excel...")

with pd.ExcelWriter(excel_output, engine='openpyxl') as writer:
    resultados_df.to_excel(writer, sheet_name='Resultados', index=False)
    
    workbook = writer.book
    worksheet = writer.sheets['Resultados']
    
    # Ajuste automático de ancho de columnas
    for i, column in enumerate(resultados_df.columns):
        column_width = max(resultados_df[column].astype(str).map(len).max(), len(str(column))) + 2
        worksheet.column_dimensions[get_column_letter(i + 1)].width = min(column_width, 50)

print(f"✅ Proceso completado con éxito.")
print(f"📊 Grupos procesados: {len(resultados)}")
print(f"📋 Total de columnas generadas: {len(resultados_df.columns)}")

# Mostrar resumen para verificación
print("\n🔍 Vista previa del Dataset para IA:")
print(resultados_df.head())
            
