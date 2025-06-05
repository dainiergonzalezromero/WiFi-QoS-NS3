# 🧠 PoFi-SDN-WiFi: Simulación de un Punto de Acceso Cognitivo con SDN y QoS (EDCA)

Este proyecto implementa un entorno de simulación en NS-3 que combina conceptos de SDN (Software Defined Networking) con redes WiFi IEEE 802.11 utilizando **QoS basado en EDCA (Enhanced Distributed Channel Access)**. Se centra en la creación de un punto de acceso inteligente (`PoFiAp`) que, junto con un controlador SDN (`PoFiController`), permite:

- Clasificación de paquetes según prioridad (ToS/DSCP).
- Encolamiento por tipo de tráfico.
- Asignación dinámica de **TXOP**.
- Recolección de métricas detalladas por clase de servicio.
- Aplicación de políticas SDN vía mensajes `PacketIn`/`FlowMod`.

---

## 🎯 Objetivos

- Simular un ecosistema WiFi cognitivo con SDN.
- Analizar el comportamiento de tráfico diferenciado (VoIP, video, Best Effort, Background).
- Evaluar métricas clave: **Throughput**, **Delay**, **Lost Packets**.

---

## 📡 PoFiAp (Access Point Inteligente)

El `PoFiAp` es una clase derivada que extiende el comportamiento de un punto de acceso WiFi para hacerlo **QoS-aware y cognitivo**:

### Funciones principales:

- **Clasificación**: analiza el campo ToS/DSCP de cada paquete recibido.
- **Asignación de Access Category (AC)**:
  - **VO (Voice)**: prioridad más alta.
  - **VI (Video)**.
  - **BE (Best Effort)**.
  - **BK (Background)**: prioridad más baja.

- **Colas por AC**: mantiene buffers separados para cada categoría.
- **TXOP dinámico**: calcula cuánto tiempo se permite transmitir a cada AC según el tráfico actual.
- **Recolección de métricas**:
  - Tiempo de retardo por paquete.
  - Paquetes perdidos por cola.
  - Throughput acumulado.
- **Generación de logs** CSV para análisis posterior.

---

## 🧠 PoFiController (Controlador SDN)

El `PoFiController` interactúa con el `PoFiAp` mediante eventos `PacketIn`. Responde con reglas `FlowMod` para dictar:

- A qué cola dirigir el paquete.
- Si se debe permitir el reenvío.
- Parámetros como el TXOP recomendado.

### Lógica:

1. Recibe paquetes de `PoFiAp`.
2. Inspecciona el ToS para determinar el tipo de tráfico.
3. Devuelve instrucciones con `FlowMod` para su tratamiento.

---

## 🎮 ¿Cómo se simula el comportamiento EDCA?

### EDCA: Enhanced Distributed Channel Access

EDCA es el mecanismo de QoS en 802.11e que permite la diferenciación de tráfico mediante 4 **Access Categories** (AC):

| Access Category | Tráfico típico     | Prioridad | Contención | TXOP típ. |
|-----------------|--------------------|-----------|------------|-----------|
| VO              | Voz, VoIP          | Muy alta  | Mínima     | Largo     |
| VI              | Video              | Alta      | Baja       | Medio     |
| BE              | Navegación, correo | Media     | Media      | Bajo      |
| BK              | Transferencias     | Baja      | Alta       | Corto     |

En esta simulación:

- Se asignan colas distintas para cada AC.
- El `PoFiAp` encola y programa la transmisión según su categoría.
- El `PoFiController` puede ajustar el **TXOP** de cada cola dinámicamente en función del tráfico observado.

---

## 📈 Métricas recolectadas

Por cada categoría de acceso y tamaño de paquete:

- **Delay promedio (ms)**.
- **Throughput acumulado (kbps)**.
- **Cantidad de paquetes perdidos**.

Estas métricas se guardan en archivos CSV por cada combinación de:

- Tamaño de paquete: `256, 512, 1024, 2048`.
- Categoría de tráfico: `VO, VI, BE, BK`.

---

## 🐍 Análisis y visualización de datos

Se proporciona una herramienta Python (Tkinter + matplotlib) que permite:

- Cargar métricas desde los CSV.
- Elegir tipo de gráfico: `líneas`, `barras`, `boxplot`.
- Filtrar por tipo de tráfico y métrica.
- Exportar la gráfica a imagen (`.png`).

---

## 🚀 Ejecución

```bash
./waf --run scratch/Finals/pofi-sim
```
## Parámetros configurables:
- Número de estaciones.
- Tamaño de paquete.
- Duración de la simulación.
- Algoritmo de asignación de TXOP.

## 📁 Estructura del proyecto
scratch/Finals/
├── sdwn.cc                 # Simulación principal
├── flowmon/                # Estadistica de los nodos por por clases de servicios
├── xml/                    # Visualizacion de las simulaciones por clases de servicios
├── estadisticas/           # Resultados en CSV por clase de servicio
└── pcap/                   # Captura de paquetes por clases de servicios

## 🛠 Requisitos
- NS-3 (verificado en versión 3.44)

## 📌 Notas adicionales
Esta simulación puede extenderse para incluir decisiones basadas en ML (p. ej. aprendizaje de patrones de tráfico).
La arquitectura es ideal para experimentos de control centralizado en entornos IoT, edge computing, etc.

## 👨‍💻 Autor
Desarrollado por Dainier Gonzalez Romero. Instituto de Ciencias e Ingenieria de la Computacion (ICIC) - Consejo Nacional de Investigaciones Científicas y Técnicas (CONICET) - Universidad Nacional del Sur (UNS)

Tesis doctoral: Diseño y desarrollo de ecosistemas digitales para la IoT cognitiva. Las redes definidas por software como herramienta de planificación


