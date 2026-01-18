# Proyecto de Control de Motor DC: Consola Digital v1.0

**Autor:** Marco Gallegos
**Fecha:** Enero 2024

---

## 1. Descripción General

Este proyecto consiste en una consola de control inteligente para un motor de corriente continua (DC). Utiliza un microcontrolador **ESP8266 (HW-364A)** para gestionar la potencia a través de un driver **DRV8871**, ofreciendo una interfaz de usuario profesional con pantalla **OLED**, encoder rotatorio y botones de dirección.

El sistema prioriza la **seguridad mecánica**, implementando rampas de aceleración y una **máquina de estados** que evita arranques accidentales o cambios de giro bruscos que puedan dañar engranajes o el motor.

---

## 2. Especificaciones de Hardware

### 2.1 Módulo de Control (Placa Azul)

La interfaz de usuario se centraliza en un módulo que incluye un **OLED de 0.96"**, un **encoder con botón** y **dos botones laterales**.

**Orden físico de los pines:**

```
CON | SDA | SCL | PSH | RTA | TRB | BAK | GND | VCC
```

---

### 2.2 Driver de Potencia (DRV8871)

* **Voltaje de Motor:** Hasta 45V (fuente externa)
* **Corriente:** Pico de 3.6A
* **Control:** PWM bidireccional (Puente H)

---

## 3. Conexiones y Pinout

### 3.1 Módulo de Interfaz a ESP8266

| Etiqueta PCB | Función               | Pin ESP8266 | Observaciones                           |
| ------------ | --------------------- | ----------- | --------------------------------------- |
| CON          | Confirmar / Adelante  | D3 (GPIO0)  | Pin de boot. No presionar al encender   |
| SDA          | I2C Data (OLED)       | D2 (GPIO4)  | —                                       |
| SCL          | I2C Clock (OLED)      | D1 (GPIO5)  | —                                       |
| PSH          | Botón Encoder (Start) | D7 (GPIO13) | —                                       |
| RTA          | Encoder Fase A        | D5 (GPIO14) | —                                       |
| TRB          | Encoder Fase B        | D6 (GPIO12) | —                                       |
| BAK          | Atrás / Reversa       | D8 (GPIO15) | Pin de boot. Debe estar LOW al encender |
| GND          | Tierra                | G (GND)     | —                                       |
| VCC          | Alimentación          | 3V (3.3V)   | —                                       |

---

### 3.2 Driver DRV8871 a ESP8266

| Etiqueta PCB | Función       | Pin ESP8266 | Observaciones             |
| ------------ | ------------- | ----------- | ------------------------- |
| IN1          | Entrada PWM A | D0 (GPIO16) | PWM limitado por hardware |
| IN2          | Entrada PWM B | D4 (GPIO2)  | Comparte LED azul interno |

---

## 4. Lógica de Operación

### 4.1 Máquina de Estados de Seguridad

Para prevenir daños y accidentes, el sistema implementa cuatro estados definidos:

* **WAITING_START (Armado):**

  * Motor bloqueado
  * Pantalla solicita pulsar **PSH** para iniciar

* **SELECT_DIR (Dirección):**

  * Espera selección del sentido
  * **CON** = Adelante
  * **BAK** = Reversa

* **RUNNING (Operación):**

  * Motor en giro
  * Encoder ajusta `targetSpeed`

* **CHANGING_DIR (Transición):**

  * Rampa baja progresivamente a 0%
  * Pausa de seguridad
  * Rampa sube en el nuevo sentido

---

### 4.2 Control de Velocidad Adaptativo

* **Giro lento:** Ajustes finos de ±1%
* **Giro rápido:** Ajustes de ±5% (intervalo entre pulsos < 50 ms)
* **Rampa de seguridad:** Incrementos de 0.5% por ciclo

---

## 5. Interfaz de Usuario (HMI)

La pantalla OLED presenta información crítica del sistema:

* **Iconos de sentido:** Flechas grandes indicando el giro
* **Velocidad real:** Porcentaje actual aplicado por la rampa
* **Velocidad objetivo:** Valor configurado por el usuario
* **Alertas del sistema:**

  * `SISTEMA BLOQUEADO`
  * `CAMBIANDO GIRO`

---

## 6. Hoja de Ruta — Versión 2 (V2)

### 6.1 Migración a ESP32

* Sustitución por **ESP32 DevKit V1**
* Eliminación de restricciones de GPIO
* PWM por hardware con mayor resolución

### 6.2 Indicadores de Estado Visuales (LED RGB)

| Color              | Estado del sistema            |
| ------------------ | ----------------------------- |
| 🟡 Amarillo        | Sistema en espera (0%)        |
| 🟢 Verde           | Motor girando adelante        |
| 🔴 Rojo            | Motor girando en reversa      |
| 🔵 Azul (parpadeo) | Cambio de dirección / frenado |

---

## 7. Limitaciones Técnicas Actuales

* **D3 / D8:** Pines sensibles al arranque por resistencias internas
* **PWM en D0:** Frecuencia no estándar, compensada por software

---

**Desarrollado por:** Marco Gallegos
