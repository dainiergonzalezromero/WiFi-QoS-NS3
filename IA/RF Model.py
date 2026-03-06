import os
import pandas as pd
import numpy as np
import logging
import gc
import psutil
import multiprocessing
import joblib
import time
import matplotlib.pyplot as plt
from datetime import datetime
from tkinter import Tk, filedialog
from typing import Dict, List

from sklearn.ensemble import RandomForestRegressor
from sklearn.model_selection import train_test_split, KFold, RandomizedSearchCV
from sklearn.metrics import r2_score, mean_absolute_error, mean_squared_error
from sklearn.preprocessing import RobustScaler, PowerTransformer
from scipy.stats.mstats import winsorize

# ==============================
# 1. CONFIGURACIÓN GLOBAL
# ==============================
TOTAL_CORES = multiprocessing.cpu_count()
MODEL_JOBS = -1 
MEMORY_THRESHOLD = 85

log_filename = f'training_master_{datetime.now().strftime("%Y%m%d_%H%M%S")}.txt'
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[logging.FileHandler(log_filename), logging.StreamHandler()]
)

# ==============================
# 2. FUNCIONES UTILITARIAS
# ==============================
def check_resources(threshold: float = MEMORY_THRESHOLD) -> bool:
    mem = psutil.virtual_memory()
    if mem.percent > threshold:
        logging.warning(f"⚠️ RAM crítica ({mem.percent}%). Liberando...")
        gc.collect()
        time.sleep(5)
        return False
    return True

def wait_for_resources():
    while not check_resources():
        time.sleep(5)

def validate_data(df: pd.DataFrame, features: List[str], targets: List[str]) -> None:
    for col in features + targets:
        if df[col].isnull().any():
            raise ValueError(f"❌ Datos nulos en: {col}")
        if np.isinf(df[col]).any():
            raise ValueError(f"❌ Valores infinitos en: {col}")
    logging.info("✅ Validación de datos completada")

# ==============================
# 3. CARGA DE DATOS
# ==============================
Tk().withdraw()
train_path = filedialog.askopenfilename(title="Selecciona Resultados_Finales.csv")
if not train_path:
    raise Exception("❌ No se seleccionó archivo.")

logging.info(f"📂 Cargando datos: {train_path}")
full_df = pd.read_csv(train_path)

# ==============================
# 4. FEATURES Y TARGETS
# ==============================
features = [
    'Packet Size','nStaWifi','nStaH','nStaM','nStaL','nStaNRT',
    'CWminH','CWmaxH','CWminM','CWmaxM','CWminL','CWmaxL','CWminNRT','CWmaxNRT'
]

priorities = ['H', 'M', 'L', 'NRT']
targets = []
for p in priorities:
    for metric in ['_mean', '_std']:
        targets.append(f'Throughput_{p}{metric}')
        targets.append(f'Delay_{p}{metric}')
    targets.append(f'LostPackets_{p}_mean')
    targets.append(f'LostPackets_{p}_sum')

validate_data(full_df, features, targets)

X = full_df[features]
y = full_df[targets]

# ==============================
# 5. SPLIT Y PREPROCESAMIENTO
# ==============================
X_train_full, X_test, y_train_full, y_test = train_test_split(
    X, y, test_size=0.2, random_state=42, shuffle=True
)

y_train_full = y_train_full.copy()
for col in targets:
    y_train_full.loc[:, col] = winsorize(y_train_full[col].values, limits=[0.005, 0.005])

output_folder = f"Master_Model"
os.makedirs(output_folder, exist_ok=True)

# ==============================
# 6. BÚSQUEDA DE HIPERPARÁMETROS
# ==============================
logging.info("🔍 Buscando hiperparámetros (RandomForest Nativo)...")
wait_for_resources()

pt_search = PowerTransformer(method='yeo-johnson').fit(y_train_full)
sc_search = RobustScaler().fit(pt_search.transform(y_train_full))
y_train_scaled = sc_search.transform(pt_search.transform(y_train_full))

param_dist = {
    'n_estimators': [100, 200, 300, 400, 500],
    'max_depth': [10, 15, 20, None],
    'min_samples_split': [2, 5, 10],
    'min_samples_leaf': [1, 2, 4],
    'max_features': ['sqrt', 0.5, 0.7],
    'bootstrap': [True]
}

rf_base = RandomForestRegressor(random_state=42, n_jobs=MODEL_JOBS)
search = RandomizedSearchCV(rf_base, param_dist, n_iter=300, cv=3, verbose=2, random_state=42, scoring='r2')
search.fit(X_train_full, y_train_scaled)
best_params = search.best_params_
logging.info(f"✅ Mejores parámetros: {best_params}")

# ==============================
# 7. VALIDACIÓN CRUZADA
# ==============================
kf = KFold(n_splits=5, shuffle=True, random_state=42)
for fold_idx, (t_idx, v_idx) in enumerate(kf.split(X_train_full), 1):
    X_t, X_v = X_train_full.iloc[t_idx], X_train_full.iloc[v_idx]
    y_t, y_v = y_train_full.iloc[t_idx], y_train_full.iloc[v_idx]
    pt = PowerTransformer(method='yeo-johnson').fit(y_t)
    sc = RobustScaler().fit(pt.transform(y_t))
    y_t_scaled = sc.transform(pt.transform(y_t))
    model = RandomForestRegressor(**best_params, n_jobs=MODEL_JOBS, random_state=42)
    model.fit(X_t, y_t_scaled)
    y_v_pred_scaled = model.predict(X_v)
    y_v_pred = pt.inverse_transform(sc.inverse_transform(y_v_pred_scaled))
    logging.info(f"Fold {fold_idx} - R2: {r2_score(y_v, y_v_pred):.4f}")
    gc.collect()

# ==============================
# 8. MODELO FINAL (USANDO 100% DE LOS DATOS)
# ==============================
logging.info("🏆 Entrenando modelo maestro final con el 100% de los datos...")
# Aplicamos winsorize a todo el set y para el entrenamiento final
y_full_winsorized = y.copy()
for col in targets:
    y_full_winsorized.loc[:, col] = winsorize(y_full_winsorized[col].values, limits=[0.05, 0.05])

pt_final = PowerTransformer(method='yeo-johnson').fit(y_full_winsorized)
sc_final = RobustScaler().fit(pt_final.transform(y_full_winsorized))
y_final_scaled = sc_final.transform(pt_final.transform(y_full_winsorized))

final_model = RandomForestRegressor(**best_params, n_jobs=MODEL_JOBS, random_state=42)
final_model.fit(X, y_final_scaled) # <--- CAMBIO: Ahora usa X e y completo

# --- Generación de Gráficas de Comparación ---
logging.info("📊 Generando gráficas de diagnóstico...")
y_pred_all_scaled = final_model.predict(X)
y_pred_all = pt_final.inverse_transform(sc_final.inverse_transform(y_pred_all_scaled))
y_pred_df = pd.DataFrame(y_pred_all, columns=targets)

metrics_to_plot = ['Delay', 'LostPackets']
priorities_to_plot = ['H', 'M']

for prio in priorities_to_plot:
    for metric in metrics_to_plot:
        col_name = f'{metric}_{prio}_mean'
        plt.figure(figsize=(8, 6))
        plt.scatter(y[col_name], y_pred_df[col_name], alpha=0.5, color='blue', s=10)
        plt.plot([y[col_name].min(), y[col_name].max()], [y[col_name].min(), y[col_name].max()], 'r--', lw=2)
        plt.title(f'Real vs Predicho: {col_name}')
        plt.xlabel('Valor Real')
        plt.ylabel('Predicción')
        plt.grid(True)
        plt.savefig(os.path.join(output_folder, f'comparacion_{col_name}.png'))
        plt.close()

# Feature Importance
feature_importance = pd.DataFrame({'feature': features, 'importance': final_model.feature_importances_}).sort_values('importance', ascending=False)
feature_importance.to_csv(os.path.join(output_folder, "feature_importance.csv"), index=False)

# Empaquetado
master_package = {
    'model': final_model,
    'scaler': sc_final,
    'transformer': pt_final,
    'features': features,
    'targets': targets,
    'metadata': {'date': datetime.now().isoformat(), 'best_params': best_params}
}
joblib.dump(master_package, os.path.join(output_folder, "Modelo Optimizado.joblib"))

logging.info(f"✅ Proceso terminado. Carpeta: {output_folder}")