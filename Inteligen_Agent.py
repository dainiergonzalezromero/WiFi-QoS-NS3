import os
import pandas as pd
import numpy as np
from sklearn.ensemble import RandomForestRegressor
import optuna
from tkinter import Tk, filedialog
import joblib
from multiprocessing import cpu_count
from optuna.visualization import plot_optimization_history, plot_param_importances

# --- 1. Ocultar ventana principal de Tkinter ---
Tk().withdraw()

# --- 2. Seleccionar archivo de TRAIN ---
train_path = filedialog.askopenfilename(
    title="Selecciona el archivo de TRAIN CSV",
    filetypes=[("CSV Files", "*.csv")]
)
if not train_path:
    raise Exception("No se seleccionó ningún archivo de train.")

# --- 3. Cargar dataset ---
train_df = pd.read_csv(train_path, sep=",")


# --- 4. Definir features y targets ---
features = [
    'Total_Devices', 'Nodes_High', 'Nodes_Medium', 'Nodes_Low', 'Nodes_NoRT',
    'CwMinH', 'CwMaxH', 'CwMinM', 'CwMaxM',
    'CwMinL', 'CwMaxL', 'CwMinNoRT', 'CwMaxNoRT'
]

targets = [
    'Throughput_H', 'Delay_H', 'Loss_Packets_H',
    'Throughput_M', 'Delay_M', 'Loss_Packets_M',
    'Throughput_L', 'Delay_L', 'Loss_Packets_L',
    'Throughput_noRT', 'Delay_noRT', 'Loss_Packets_noRT'
]

# --- 5. Guardar modelo y resultados ---
model_path = "Modelo_Estudios_Logs/RF_Model.pkl"
updated_folder =  "Modelo_Estudios_Logs"
os.makedirs(updated_folder, exist_ok=True)

# --- 5. Validar datos ---
if train_df[features + targets].isnull().any().any():
    raise ValueError("El dataset contiene valores nulos. Por favor, límpialos antes de entrenar.")

X_train = train_df[features]
y_train = train_df[targets]

# --- 6. Entrenar modelo ---
rf = RandomForestRegressor(n_estimators=500, random_state=42)
rf.fit(X_train, y_train)

# Guardar el modelo entrenado
joblib.dump(rf, model_path)

# Cargar el modelo entrenado
rf = joblib.load(model_path)

# --- 7. Definir escenario representativo ---
escenario = {
    'Total_Devices': 50,
    'Nodes_High': 12,
    'Nodes_Medium': 12,
    'Nodes_Low': 13,
    'Nodes_NoRT': 13
}

# --- 8. Posibles valores CW ---
cw_values = [7, 15, 31, 63, 255, 1023]
n_cw = len(cw_values)

# --- 9. Función para muestrear CWmin y CWmax ---
def sample_cw(trial, prefix):
    idx_min = trial.suggest_int(f"{prefix}_idx_min", 0, n_cw - 2)
    idx_max = trial.suggest_int(f"{prefix}_idx_max", idx_min + 1, n_cw - 1)
    return cw_values[idx_min], cw_values[idx_max]

# --- 10. Función objetivo (ponderaciones personalizadas) ---
def objective(trial):
    alta = sample_cw(trial, 'H')
    media = sample_cw(trial, 'M')
    baja = sample_cw(trial, 'L')
    noRT = sample_cw(trial, 'noRT')

    x_eval = pd.DataFrame([{
        **escenario,
        'CwMinH': alta[0], 'CwMaxH': alta[1],
        'CwMinM': media[0], 'CwMaxM': media[1],
        'CwMinL': baja[0], 'CwMaxL': baja[1],
        'CwMinNoRT': noRT[0], 'CwMaxNoRT': noRT[1],
    }])

    y_pred = rf.predict(x_eval)[0]

    # --- Índices de Delay y Loss ---
    # H: Delay=1, Loss=2
    # M: Delay=4, Loss=5
    # L: Delay=7, Loss=8
    # noRT: Delay=10, Loss=11

    y_pred_max_loss = np.max(np.array([y_pred[2], y_pred[5], y_pred[8],y_pred[11]]))
    y_pred_max_delay = np.max(np.array([y_pred[1], y_pred[4], y_pred[7],y_pred[10]]))

    score = (
        0.2 * (y_pred[2] + y_pred[5])/y_pred_max_loss   +  # Loss_H y M
        0.25 *(y_pred[1] + y_pred[4])/y_pred_max_delay  +  # Delay_H
        0.005 *(y_pred[8] + y_pred[11])/y_pred_max_loss  +  # Loss_L
        0.005 *(y_pred[7] + y_pred[10])/y_pred_max_delay    # Delay_L
    )

    return score

# --- 11. Crear estudio Optuna ---
study = optuna.create_study(
    direction='minimize',
    sampler=optuna.samplers.TPESampler(seed=42),
)

# --- 12. Definir condiciones iniciales ---
initial_params = {
    'H_idx_min': 0,  # CWminH = 7
    'H_idx_max': 1,  # CWmaxH = 15
    'M_idx_min': 0,  # CWminM = 7
    'M_idx_max': 1,  # CWmaxM = 15
    'L_idx_min': 1,  # CWminL = 15
    'L_idx_max': 5,  # CWmaxL = 1023
    'noRT_idx_min': 1,  # CWminNoRT = 15
    'noRT_idx_max': 5   # CWmaxNoRT = 1023
}

# Encolar el trial inicial
print("🎯 Encolando configuración inicial personalizada...")
study.enqueue_trial(initial_params)

# --- 12. Ejecutar optimización ---
num_cpus = 1
numero_trials = 500
print(f"🚀 Iniciando optimización global ({numero_trials} trials, {num_cpus} CPU threads)...")
study.optimize(objective, n_trials=numero_trials, n_jobs=num_cpus)

# --- 13. Extraer mejor configuración ---
best_params = study.best_params
best_cw = {}
for prio in ['H', 'M', 'L', 'noRT']:
    cwmin = cw_values[best_params[f'{prio}_idx_min']]
    cwmax = cw_values[best_params[f'{prio}_idx_max']]
    best_cw[prio] = (cwmin, cwmax)

# --- 14. Mostrar resultados ---
print("\n✅ Optimización completada.")
print("📊 Mejor combinación CW global (ponderaciones personalizadas):")
print(f"TOTAL_DEVICES = {escenario['Total_Devices']}")
print(f"nStaH = {escenario['Nodes_High']}")
print(f"nStaM = {escenario['Nodes_Medium']}")
print(f"nStaL = {escenario['Nodes_Low']}")
print(f"nStaNRT = {escenario['Nodes_NoRT']}\n")

print("cw_params = {")
for prio, vals in best_cw.items():
    print(f"    '{prio}': ({vals[0]}, {vals[1]}),")
print("}")

print(f"\n🏁 Score mínimo calculado: {study.best_value:.6f}")

joblib.dump(study, f"Modelo_Estudios_Logs/Optuna_Study_{escenario['Total_Devices']}.pkl")
print(f"\n💾 Modelo y estudio guardados como 'RF_Model.pkl' y 'Optuna_Study_{escenario['Total_Devices']}.pkl'.")

# --- 16. Exportar y visualizar resultados ---
df_trials = study.trials_dataframe()
df_trials.to_csv(f"Modelo_Estudios_Logs/Optuna_Trials_Log_{escenario['Total_Devices']}.csv", index=False)
print(f"📈 Registro completo guardado como 'Optuna_Trials_Log_{escenario['Total_Devices']}.csv'.")

# --- 17. Gráficas interactivas ---
try:
    print("\n📊 Generando visualizaciones interactivas...")
    fig1 = plot_optimization_history(study)
    fig2 = plot_param_importances(study)

    fig1.show()
    fig2.show()
    print("✅ Gráficas generadas (historial e importancia de parámetros).")
except Exception as e:
    print(f"⚠️ No se pudieron generar las gráficas automáticamente: {e}")

