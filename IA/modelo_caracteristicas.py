import joblib
import os

# 1. Ruta al archivo generado
file_path = os.path.join("Master_Model", "Modelo Optimizado.joblib")

try:
    # 2. Cargar el paquete maestro
    package = joblib.load(file_path)
    
    # 3. Extraer los mejores parámetros de los metadatos
    best_params = package['metadata']['best_params']
    
    # 4. Obtener el modelo y su configuración completa
    final_model = package['model']
    all_params = final_model.get_params()

    print("="*50)
    print("🏆 HIPERPARÁMETROS GANADORES (RandomizedSearch)")
    print("="*50)
    for param, value in best_params.items():
        print(f"{param:20}: {value}")

    print("\n" + "="*50)
    print("🌲 CONFIGURACIÓN COMPLETA DEL MODELO FINAL")
    print("="*50)
    # Mostramos algunos que no estaban en la búsqueda pero son importantes
    print(f"{'n_jobs':20}: {all_params['n_jobs']}")
    print(f"{'criterion':20}: {all_params['criterion']}")
    print(f"{'random_state':20}: {all_params['random_state']}")
    print(f"{'oob_score':20}: {all_params['oob_score']}")
    print("="*50)

except FileNotFoundError:
    print(f"❌ No se encontró el archivo en: {file_path}")
except Exception as e:
    print(f"❌ Ocurrió un error: {e}")