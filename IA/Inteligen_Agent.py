import os
import pandas as pd
import numpy as np
import optuna
from tkinter import Tk, filedialog
import joblib
import itertools
import time
import logging
from datetime import datetime
import warnings

# --- Configuración Inicial ---
warnings.filterwarnings('ignore')
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler(f'optimization_log_{datetime.now().strftime("%Y%m%d_%H%M%S")}.txt'),
        logging.StreamHandler()
    ]
)

Tk().withdraw()

# ==============================
# 1. CARGAR MODELO
# ==============================
model_path = filedialog.askopenfilename(
    title="Selecciona el modelo optimizado (.joblib)",
    filetypes=[("Joblib Files", "*.joblib")]
)
if not model_path:
    raise Exception("No se seleccionó ningún modelo.")

logging.info(f"📦 Cargando modelo maestro desde: {model_path}")
package = joblib.load(model_path)

rf_model = package['model']
scaler = package['scaler']
pt = package['transformer']
features = package['features']
targets = package['targets']

# Valores posibles para Contention Window (802.11)
cw_values = [3, 7, 15, 31, 63, 127, 255, 511, 1023]
n_cw = len(cw_values)

# ==============================
# 2. ESCENARIOS A OPTIMIZAR
# ==============================
packet_sizes = [1024]
device_configs = [
    # (10, 3, 3, 2, 2), (20, 5, 5, 5, 5), (30, 8, 8, 7, 7), (40, 10, 10, 10, 10), (50, 13, 13, 12, 12),
    (100, 25, 25, 25, 25),
    (100, 40, 30, 20, 10)
    # (70, 18, 18, 17, 17), (80, 20, 20, 20, 20), (90, 23, 23, 22, 22), (100, 25, 25, 25, 25)
]

# ==============================
# 3. FUNCIÓN DE PREDICCIÓN
# ==============================
def predict_with_master(X_new: pd.DataFrame):
    X_new = X_new[features].copy()
    preds_scaled = rf_model.predict(X_new)
    preds_trans = scaler.inverse_transform(preds_scaled)
    preds_original = pt.inverse_transform(preds_trans)
    return preds_original

# ==============================
# 4. FUNCIÓN OBJETIVO
# ==============================
def objective(trial, escenario):
    cw_params = {}
    for p in ['H', 'M', 'L', 'NRT']:
        idx_min = trial.suggest_int(f"{p}_idx_min", 0, n_cw - 2)
        idx_max = trial.suggest_int(f"{p}_idx_max", idx_min + 1, n_cw - 1)
        cw_params[f'CWmin{p}'] = cw_values[idx_min]
        cw_params[f'CWmax{p}'] = cw_values[idx_max]

    input_data = pd.DataFrame([{**escenario, **cw_params}])
    y_pred_real = predict_with_master(input_data)[0]
    res = dict(zip(targets, y_pred_real))

    def calc_priority_cost(p, weight):
        # Ajustado a tus nombres de columnas: _mean, _min, _max, _std
        delay_cost = ( res[f'Delay_{p}_mean'] * 0.4 + res[f'Delay_{p}_std'] * 0.2 )
        # Lost Packets usa _mean para el costo
        loss_cost = res[f'LostPackets_{p}_mean'] * 0.4
        return weight * (delay_cost + loss_cost)

    total_cost = ( calc_priority_cost('H', 0.35) + calc_priority_cost('M', 0.35) + calc_priority_cost('L', 0.30) )

    return total_cost

# ==============================
# 5. BUCLE PRINCIPAL
# ==============================

output_folder = "Resultados_Optimizacion_IA_Test"
os.makedirs(output_folder, exist_ok=True)
resultados_finales = []

for ps, config in itertools.product(packet_sizes, device_configs):
    escenario = {
        'Packet Size': ps, 'nStaWifi': config[0],
        'nStaH': config[1], 'nStaM': config[2],
        'nStaL': config[3], 'nStaNRT': config[4]
    }

    study = optuna.create_study(direction='minimize', sampler=optuna.samplers.TPESampler(seed=42))
    study.optimize(lambda t: objective(t, escenario), n_trials=500, n_jobs=-1)

    best = study.best_params
    row = {**escenario, 'Score': study.best_value}

    for p in ['H', 'M', 'L', 'NRT']:
        row[f'CWmin{p}'] = cw_values[best[f'{p}_idx_min']]
        row[f'CWmax{p}'] = cw_values[best[f'{p}_idx_max']]

    # Predicción final para reporte
    x_best = pd.DataFrame([row])[features]
    y_final = predict_with_master(x_best)[0]
    res_final = dict(zip(targets, y_final))

    for p in ['H', 'M', 'L', 'NRT']:
        row[f'Delay_{p}_mean'] = res_final[f'Delay_{p}_mean']
        row[f'Delay_{p}_std'] = res_final[f'Delay_{p}_std']
        row[f'Loss_{p}_pct'] = res_final[f'LostPackets_{p}_mean']

    resultados_finales.append(row)

# ==============================
# 6. GUARDADO FINAL
# ==============================
df_res = pd.DataFrame(resultados_finales)
df_res.to_csv(os.path.join(output_folder, "Optimizacion_CW_Final_Caso_2.csv"), index=False)
df_res.to_excel(os.path.join(output_folder, "Optimizacion_CW_Final_Caso_2.xlsx"), index=False)

logging.info(f"✅ Proceso completado.")