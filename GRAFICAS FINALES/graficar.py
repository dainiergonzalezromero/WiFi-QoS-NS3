import pandas as pd
import matplotlib.pyplot as plt
import os
import numpy as np
from matplotlib.backends.backend_pdf import PdfPages

def graficar_interactivo():
    """
    Versión interactiva con selección múltiple de configuraciones
    """
    archivo_ods = r"RESULTADO_FINAL.xlsx"
    
    if not os.path.exists(archivo_ods):
        print(f"El archivo {archivo_ods} no existe.")
        return

    # NUEVO: opción de exportación
    print("\n¿Deseas exportar todas las gráficas a PDF?")
    print("1. Sí (exportar por Packet Size y Prioridad)")
    print("2. No (modo interactivo normal)")

    modo_exportar = input("Selecciona opción: ").strip()

    if modo_exportar == "1":
        exportar_todas_las_graficas(archivo_ods)
        return
    
    # Mostrar hojas disponibles
    hojas = ['Delay', 'LostPackets', 'Throughput']
    print("\nHojas disponibles:")
    for i, hoja in enumerate(hojas, 1):
        print(f"{i}. {hoja}")
    
    # Seleccionar hoja
    opcion_hoja = int(input("\nSelecciona el número de la hoja: ")) - 1
    hoja_seleccionada = hojas[opcion_hoja]
    
    # Leer la hoja
    df = leer_archivo_ods(archivo_ods, hoja_seleccionada)
    
    if df is None:
        return
    
    # Mostrar configuraciones disponibles
    configs_disponibles = list(df['Configuration'].unique())
    print("\nConfiguraciones disponibles:")
    for i, config in enumerate(configs_disponibles, 1):
        print(f"{i}. {config}")
    print(f"{len(configs_disponibles) + 1}. Todas")
    print("0. Selección personalizada (ej: 1,2,3 o 1-3)")
    
    # Seleccionar configuración(es)
    opcion_config = input("\nSelecciona configuración(es): ").strip()
    
    configs_seleccionadas = procesar_seleccion_configuraciones(opcion_config, configs_disponibles)
    
    if not configs_seleccionadas:
        print("No se seleccionaron configuraciones válidas. Saliendo...")
        return
    
    print(f"\nConfiguraciones seleccionadas: {', '.join(configs_seleccionadas)}")
    
    # Mostrar packet sizes disponibles
    sizes_disponibles = sorted(df['Packet Size'].unique())
    print("\nPacket Sizes:")
    for i, size in enumerate(sizes_disponibles, 1):
        print(f"{i}. {size}")
    
    # Seleccionar packet size
    opcion_size = int(input("\nSelecciona Packet Size: ")) - 1
    packet_size_seleccionado = sizes_disponibles[opcion_size]
    
    # Mostrar prioridades disponibles
    prioridades_disponibles = ['H', 'M', 'L', 'NRT']
    print("\nPrioridades:")
    for i, prioridad in enumerate(prioridades_disponibles, 1):
        nombre_prioridad = {
            'H': 'Alta Prioridad (H)',
            'M': 'Media Prioridad (M)',
            'L': 'Baja Prioridad (L)',
            'NRT': 'No Tiempo Real (NRT)'
        }[prioridad]
        print(f"{i}. {nombre_prioridad}")
    print("0. Todas las prioridades")
    
    # Seleccionar prioridad(es)
    opcion_prioridad = input("\nSelecciona Prioridad(es): ").strip()
    
    prioridades_seleccionadas = procesar_seleccion_prioridades(opcion_prioridad, prioridades_disponibles)
    
    if not prioridades_seleccionadas:
        print("No se seleccionaron prioridades válidas. Saliendo...")
        return
    
    print(f"\nPrioridades seleccionadas: {', '.join(prioridades_seleccionadas)}")
    
    # Preguntar tipo de gráfico
    print("\nTipo de gráfico:")
    print("1. Comparar configuraciones (una prioridad)")
    print("2. Comparar prioridades (una configuración)")
    print("3. Matriz de comparación (múltiples configs y prioridades)")
    
    tipo_grafico = int(input("\nSelecciona tipo de gráfico: "))
    
    if tipo_grafico == 1:
        # Comparar configuraciones para una prioridad
        if len(prioridades_seleccionadas) > 1:
            print("Para comparar configuraciones, selecciona solo una prioridad")
            prioridad = prioridades_seleccionadas[0]
            print(f"Usando prioridad: {prioridad}")
        else:
            prioridad = prioridades_seleccionadas[0]
        
        graficar_comparacion_configuraciones(df, hoja_seleccionada, 
                                            configs_seleccionadas, 
                                            packet_size_seleccionado,
                                            prioridad)
    
    elif tipo_grafico == 2:
        # Comparar prioridades para una configuración
        if len(configs_seleccionadas) > 1:
            print("Para comparar prioridades, selecciona solo una configuración")
            configuracion = configs_seleccionadas[0]
            print(f"Usando configuración: {configuracion}")
        else:
            configuracion = configs_seleccionadas[0]
        
        graficar_comparacion_prioridades(df, hoja_seleccionada,
                                        configuracion,
                                        packet_size_seleccionado,
                                        prioridades_seleccionadas)
    
    elif tipo_grafico == 3:
        # Matriz de comparación
        graficar_matriz_comparacion(df, hoja_seleccionada,
                                   configs_seleccionadas,
                                   packet_size_seleccionado,
                                   prioridades_seleccionadas)
def exportar_todas_las_graficas(archivo_ods):
    """
    Exporta UN PDF POR CADA GRÁFICA:
    Formato: Hoja_PacketSize_Prioridad.pdf
    Ejemplo: Delay_256_H.pdf
    
    Cada PDF contiene:
    - Todas las configuraciones
    - X: Devices
    - Y: Variable de la hoja
    - Intervalo de confianza
    """
    hojas = ['Delay', 'LostPackets', 'Throughput']
    prioridades = ['H', 'M', 'L', 'NRT']
    
    carpeta_salida = "PDF_Graficas"
    os.makedirs(carpeta_salida, exist_ok=True)
    
    print("\nIniciando exportación: 1 PDF por gráfica...")
    
    total = 0
    
    for hoja in hojas:
        print(f"\nProcesando hoja: {hoja}")
        
        df = leer_archivo_ods(archivo_ods, hoja)
        if df is None:
            continue
        
        configuraciones = list(df['Configuration'].unique())
        packet_sizes = sorted(df['Packet Size'].unique())
        
        for packet_size in packet_sizes:
            for prioridad in prioridades:
                
                fig = generar_figura_configuraciones(
                    df=df,
                    nombre_hoja=hoja,
                    configuraciones=configuraciones,
                    packet_size=packet_size,
                    prioridad=prioridad
                )
                
                if fig is None:
                    continue
                
                # Nombre del archivo: Hoja_Size_Prioridad.pdf
                mapa = {'H': 'HIGH', 'M': 'MEDIUM', 'L': 'LOW', 'NRT': 'NRT'}
                nombre_archivo = f"{hoja}_{packet_size}_{mapa[prioridad]}.pdf"
                
                ruta_pdf = os.path.join(carpeta_salida, nombre_archivo)
                
                # Guardar UN PDF por figura
                with PdfPages(ruta_pdf) as pdf:
                    pdf.savefig(fig, bbox_inches='tight')
                
                plt.close(fig)
                total += 1
                print(f"Exportado: {nombre_archivo}")
    
    print(f"\nExportación completada. Total de PDFs generados: {total}")

def generar_figura_configuraciones(df, nombre_hoja, configuraciones, packet_size, prioridad):
    
    # Genera la figura (sin mostrar) para exportar a PDF
    
    col_mean = f'Mean {prioridad}'
    col_std = f'Std {prioridad}'
    
    fig = plt.figure(figsize=(14, 8))
    
    colores = plt.cm.tab10(np.linspace(0, 1, len(configuraciones)))
    estilos_linea = ['-', '--', '-.', ':']
    
    hay_datos = False
    
    for i, config in enumerate(configuraciones):
        df_filtrado = df[
            (df['Configuration'] == config) &
            (df['Packet Size'] == packet_size)
        ].copy()
        
        if df_filtrado.empty:
            continue
        
        hay_datos = True
        df_filtrado = df_filtrado.sort_values('Devices')
        
        x = df_filtrado['Devices'].values
        y = df_filtrado[col_mean].values
        y_err = calcular_intervalo_confianza(df_filtrado[col_std].values)
        
        color = colores[i % len(colores)]
        estilo = estilos_linea[i % len(estilos_linea)]
        
        # Línea principal
        plt.plot(
            x, y,
            label=config,
            color=color,
            linewidth=2.5,
            marker='o',
            markersize=6,
            linestyle=estilo
        )
        
        # Intervalo de confianza
        plt.fill_between(
            x,
            y - y_err,
            y + y_err,
            color=color,
            alpha=0.2
        )
    
    if not hay_datos:
        plt.close(fig)
        return None
    
    nombre_prioridad = {
        'H': 'HIGH PRIORITY (H)',
        'M': 'MEDIUM PRIORITY (M)',
        'L': 'LOW PRIORITY (L)',
        'NRT': 'NO REAL TIME (NRT)'
    }[prioridad]
    
    titulo = f'{nombre_hoja} - {nombre_prioridad} | Packet Size: {packet_size}'
    nombre_hoja = '% of Packet Loss' if nombre_hoja == 'LostPackets' else f"MEAN VALUE OF {nombre_hoja}"
    # plt.title(titulo, fontsize=16, fontweight='bold')
    plt.xlabel('NUMBER OF DEVICES', fontsize=18)
    plt.ylabel(f'{nombre_hoja.upper()}', fontsize=18)
    plt.tick_params(axis='both', labelsize=16)
    plt.grid(True, alpha=0.3, linestyle='--')
    # plt.legend(loc='best', fontsize=9)
    plt.ylim(bottom=0)
    
    devices_unicos = sorted(df[df['Packet Size'] == packet_size]['Devices'].unique())
    plt.xticks(devices_unicos)
    
    plt.tight_layout()
    
    return fig



def procesar_seleccion_configuraciones(opcion, configs_disponibles):
    """
    Procesa la selección de configuraciones que puede ser:
    - Número individual (ej: "1")
    - Múltiples números separados por comas (ej: "1,2,3")
    - Rango (ej: "1-3")
    - "0" para selección personalizada
    - Número + 1 para "Todas"
    """
    opcion = opcion.strip()
    
    # Caso "Todas"
    if opcion == str(len(configs_disponibles) + 1):
        return configs_disponibles
    
    # Caso selección personalizada
    if opcion == "0":
        seleccion = input("Ingresa los números de las configuraciones (ej: 1,2,3 o 1-3): ").strip()
        return procesar_seleccion_configuraciones(seleccion, configs_disponibles)
    
    # Procesar rangos (ej: "1-3")
    if '-' in opcion:
        try:
            inicio, fin = map(int, opcion.split('-'))
            indices = list(range(inicio - 1, fin))
            return [configs_disponibles[i] for i in indices if 0 <= i < len(configs_disponibles)]
        except:
            print(f"Formato de rango inválido: {opcion}")
            return []
    
    # Procesar lista separada por comas
    if ',' in opcion:
        indices = []
        for parte in opcion.split(','):
            parte = parte.strip()
            if '-' in parte:
                # Manejar rangos dentro de la lista (ej: "1,3-5,7")
                try:
                    inicio, fin = map(int, parte.split('-'))
                    indices.extend(list(range(inicio - 1, fin)))
                except:
                    print(f"Formato de rango inválido: {parte}")
            else:
                try:
                    idx = int(parte) - 1
                    if 0 <= idx < len(configs_disponibles):
                        indices.append(idx)
                except:
                    print(f"Valor inválido: {parte}")
        
        return [configs_disponibles[i] for i in sorted(set(indices)) if 0 <= i < len(configs_disponibles)]
    
    # Número individual
    try:
        idx = int(opcion) - 1
        if 0 <= idx < len(configs_disponibles):
            return [configs_disponibles[idx]]
    except:
        pass
    
    print(f"Selección inválida: {opcion}")
    return []

def procesar_seleccion_prioridades(opcion, prioridades_disponibles):
    """
    Procesa la selección de prioridades similar a configuraciones
    """
    opcion = opcion.strip()
    
    # Caso "Todas" (opción 0)
    if opcion == "0":
        return prioridades_disponibles
    
    # Procesar rangos
    if '-' in opcion:
        try:
            inicio, fin = map(int, opcion.split('-'))
            indices = list(range(inicio - 1, fin))
            return [prioridades_disponibles[i] for i in indices if 0 <= i < len(prioridades_disponibles)]
        except:
            print(f"Formato de rango inválido: {opcion}")
            return []
    
    # Procesar lista separada por comas
    if ',' in opcion:
        indices = []
        for parte in opcion.split(','):
            parte = parte.strip()
            if '-' in parte:
                try:
                    inicio, fin = map(int, parte.split('-'))
                    indices.extend(list(range(inicio - 1, fin)))
                except:
                    print(f"Formato de rango inválido: {parte}")
            else:
                try:
                    idx = int(parte) - 1
                    if 0 <= idx < len(prioridades_disponibles):
                        indices.append(idx)
                except:
                    print(f"Valor inválido: {parte}")
        
        return [prioridades_disponibles[i] for i in sorted(set(indices))]
    
    # Número individual
    try:
        idx = int(opcion) - 1
        if 0 <= idx < len(prioridades_disponibles):
            return [prioridades_disponibles[idx]]
    except:
        pass
    
    print(f"Selección inválida: {opcion}")
    return []

def leer_archivo_ods(nombre_archivo, hoja):
    """
    Lee una hoja específica del archivo ODS
    """
    try:
        df = pd.read_excel(nombre_archivo, sheet_name=hoja)
        df.columns = df.columns.str.strip()
        
        columnas_numericas = ['Mean H', 'Std H', 'Mean M', 'Std M', 
                             'Mean L', 'Std L', 'Mean NRT', 'Std NRT']
        
        for col in columnas_numericas:
            if col in df.columns:
                df[col] = df[col].astype(str).str.replace(',', '.').astype(float)
        
        return df
    except Exception as e:
        print(f"Error al leer el archivo: {e}")
        return None

def calcular_intervalo_confianza(std, n=10, confianza=0.95):
    """
    Calcula el intervalo de confianza usando t-student
    Por defecto: n=10 (grados de libertad=9), confianza=95% (t=2.2622)
    """
    # Valores t-student para diferentes niveles de confianza y n=10
    # gl = n-1 = 9
    t_valores = {
        0.90: 1.833,
        0.95: 2.2622,
        0.99: 3.250
    }
    
    t = t_valores.get(confianza, 2.2622)  # Por defecto 95%
    return (t / np.sqrt(n)) * std

def graficar_comparacion_configuraciones(df, nombre_hoja, configuraciones, packet_size, prioridad):
    
    # Genera una gráfica comparando múltiples configuraciones para una prioridad específica
    
    plt.figure(figsize=(14, 8))
    
    col_mean = f'Mean {prioridad}'
    col_std = f'Std {prioridad}'
    
    # Paleta de colores
    colores = plt.cm.tab10(np.linspace(0, 1, len(configuraciones)))
    estilos_linea = ['-', '--', '-.', ':']
    
    for i, config in enumerate(configuraciones):
        df_filtrado = df[(df['Configuration'] == config) & 
                         (df['Packet Size'] == packet_size)].copy()
        
        if df_filtrado.empty:
            print(f"Advertencia: No hay datos para {config}")
            continue
        
        df_filtrado = df_filtrado.sort_values('Devices')
        
        x = df_filtrado['Devices'].values
        y = df_filtrado[col_mean].values
        # Calcular intervalo de confianza (asumiendo n=10 muestras)
        y_err = calcular_intervalo_confianza(df_filtrado[col_std].values)
        
        color = colores[i % len(colores)]
        estilo = estilos_linea[i % len(estilos_linea)]
        
        # Línea principal
        plt.plot(x, y, 
                label=config,
                color=color,
                linewidth=2.5,
                marker='o',
                markersize=8,
                linestyle=estilo)
        
        # Área sombreada para el intervalo de confianza
        plt.fill_between(x, 
                        y - y_err, 
                        y + y_err,
                        color=color,
                        alpha=0.2,
                        linewidth=0)
        
        # Bordes del área
        plt.plot(x, y - y_err, color=color, linestyle=':', linewidth=1, alpha=0.5)
        plt.plot(x, y + y_err, color=color, linestyle=':', linewidth=1, alpha=0.5)
    
    nombre_prioridad = {
        'H': 'HIGH PRIORITY (H)',
        'M': 'MEDIUM PRIORITY (M)',
        'L': 'LOW PRIORITY (L)',
        'NRT': 'NO REAL TIME (NRT)'
    }[prioridad]
    
    titulo = f'{nombre_hoja} - {nombre_prioridad}\nPacket Size: {packet_size}'
    nombre_hoja = '% of Packet Loss' if nombre_hoja == 'LostPackets' else f"MEAN VALUE OF {nombre_hoja}"
    # plt.title(titulo, fontsize=16, fontweight='bold')
    
    plt.xlabel('NUMBER OF DEVICES', fontsize=18)
    plt.ylabel(f'{nombre_hoja.upper()}', fontsize=18)
    plt.tick_params(axis='both', labelsize=16)
    plt.grid(True, alpha=0.3, linestyle='--')
    # plt.legend(loc='best', fontsize=10)
    plt.ylim(bottom=0)
    
    devices_unicos = sorted(df[df['Packet Size'] == packet_size]['Devices'].unique())
    plt.xticks(devices_unicos)
    
    plt.tight_layout()
    plt.show()

def graficar_comparacion_prioridades(df, nombre_hoja, configuracion, packet_size, prioridades):
    """
    Genera una gráfica comparando múltiples prioridades para una configuración específica
    """
    plt.figure(figsize=(14, 8))
    
    info_prioridades = {
        'H': {'label': 'HIGH PRIORITY (H)', 'marker': 'o'},
        'M': {'label': 'MEDIUM PRIORITY (M)', 'marker': 's'},
        'L': {'label': 'LOW PRIORITY (L)', 'marker': '^'},
        'NRT': {'label': 'NO REAL TIME (NRT)', 'marker': 'd'}
    }
    
    colores = plt.cm.Set1(np.linspace(0, 1, len(prioridades)))
    
    for i, prioridad in enumerate(prioridades):
        col_mean = f'Mean {prioridad}'
        col_std = f'Std {prioridad}'
        
        df_filtrado = df[(df['Configuration'] == configuracion) & 
                         (df['Packet Size'] == packet_size)].copy()
        
        if df_filtrado.empty:
            print(f"Advertencia: No hay datos para {prioridad}")
            continue
        
        df_filtrado = df_filtrado.sort_values('Devices')
        
        x = df_filtrado['Devices'].values
        y = df_filtrado[col_mean].values
        y_err = calcular_intervalo_confianza(df_filtrado[col_std].values)
        
        color = colores[i % len(colores)]
        
        # Línea principal
        plt.plot(x, y, 
                label=info_prioridades[prioridad]['label'],
                color=color,
                linewidth=2.5,
                marker=info_prioridades[prioridad]['marker'],
                markersize=8)
        
        # Área sombreada para el intervalo de confianza
        plt.fill_between(x, 
                        y - y_err, 
                        y + y_err,
                        color=color,
                        alpha=0.2,
                        linewidth=0)
    
    titulo = f'{nombre_hoja} - {configuracion}\nPacket Size: {packet_size}'
    
    # plt.title(titulo, fontsize=16, fontweight='bold')
    nombre_hoja = '% of Packet Loss' if nombre_hoja == 'LostPackets' else f"MEAN VALUE OF {nombre_hoja}"

    plt.xlabel('NUMBER OF DEVICES', fontsize=18)
    plt.ylabel(f'MEAN VALUE OF {nombre_hoja.upper()}', fontsize=18)
    plt.tick_params(axis='both', labelsize=16)
    plt.grid(True, alpha=0.3, linestyle='--')
    # plt.legend(loc='best', fontsize=10)
    plt.ylim(bottom=0)
    
    devices_unicos = sorted(df[df['Packet Size'] == packet_size]['Devices'].unique())
    plt.xticks(devices_unicos)
    
    plt.tight_layout()
    plt.show()

def graficar_matriz_comparacion(df, nombre_hoja, configuraciones, packet_size, prioridades):
    """
    Genera una matriz de subplots comparando configuraciones y prioridades
    """
    n_configs = len(configuraciones)
    n_prioridades = len(prioridades)
    
    fig, axes = plt.subplots(n_prioridades, n_configs, 
                             figsize=(5*n_configs, 4*n_prioridades),
                             squeeze=False)
    
    info_prioridades = {
        'H': 'HIGH PRIORITY (H)',
        'M': 'MEDIUM PRIORITY (M)',
        'L': 'LOW PRIORITY (L)',
        'NRT': 'NO REAL TIME (NRT)'
    }
    
    colores = plt.cm.tab10(np.linspace(0, 1, n_configs))
    
    for i, prioridad in enumerate(prioridades):
        for j, config in enumerate(configuraciones):
            ax = axes[i, j]
            
            col_mean = f'Mean {prioridad}'
            col_std = f'Std {prioridad}'
            
            df_filtrado = df[(df['Configuration'] == config) & 
                             (df['Packet Size'] == packet_size)].copy()
            
            if not df_filtrado.empty:
                df_filtrado = df_filtrado.sort_values('Devices')
                
                x = df_filtrado['Devices'].values
                y = df_filtrado[col_mean].values
                y_err = calcular_intervalo_confianza(df_filtrado[col_std].values)
                
                color = colores[j % len(colores)]
                
                # Línea principal
                ax.plot(x, y, 
                       color=color,
                       linewidth=2,
                       marker='o',
                       markersize=6)
                
                # Área sombreada
                ax.fill_between(x, 
                               y - y_err, 
                               y + y_err,
                               color=color,
                               alpha=0.2,
                               linewidth=0)
            
            # Configurar subplot
            if i == 0:
                ax.set_title(f'{config}', fontsize=12, fontweight='bold')
            if j == 0:
                ax.set_ylabel(f'{info_prioridades[prioridad]}', fontsize=11)
            
            ax.grid(True, alpha=0.3, linestyle='--')
            ax.set_ylim(bottom=0)
            ax.set_xticks(sorted(df[df['Packet Size'] == packet_size]['Devices'].unique()))
            
            # Rotar etiquetas x si hay muchas
            if n_configs > 2:
                ax.tick_params(axis='x', rotation=45)
    
    # Título general
    # fig.suptitle(f'{nombre_hoja} - Packet Size: {packet_size}', fontsize=16, fontweight='bold', y=1.02)
    
    plt.tight_layout()
    plt.show()

if __name__ == "__main__":
    graficar_interactivo()
