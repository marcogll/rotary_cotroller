<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://raw.githubusercontent.com/marcogll/mg_data_storage/b1b4035928e086f9394baf9988f80f4b0075ae20/soul23/logo/s23_logo_wh.png">
    <img src="https://raw.githubusercontent.com/marcogll/mg_data_storage/b1b4035928e086f9394baf9988f80f4b0075ae20/soul23/logo/s23_logo_blk.png" alt="Soul23" width="110">
  </picture>
</p>

<h1 align="center">rotary_cotroller.git</h1>

<p align="center">
  - Máquina de estados para evitar arranques accidentales.
</p>

<p align="center">
  [![Soul23](https://img.shields.io/badge/Soul23-000000?style=for-the-badge)](https://github.com/marcogll)
</p>


## 1. Descripción General

Este proyecto implementa una consola digital de control para un motor de corriente continua (DC), orientada a operación segura, control fino de velocidad y protección mecánica/eléctrica. A diferencia de un control directo con potenciómetro, el sistema introduce:

- Máquina de estados para evitar arranques accidentales.
- Aceleración y desaceleración progresiva (rampa).
- Inversión de giro con secuencia controlada.
- Interfaz hombre–máquina con encoder rotatorio, botones y pantalla OLED.

El sistema se desarrolla inicialmente sobre **HW-364A (ESP8266)**, con una ruta clara de migración a **ESP32 DevKit V1**.

---

## 2. Arquitectura del Sistema

### 2.1 Flujo Funcional

```
Usuario (Encoder / Botones)
        ↓
     ESP8266
        ↓
    Driver DRV8871
        ↓
     Motor DC
        ↓
   OLED (Feedback)
```

---

## 3. Hardware

### 3.1 Controlador Principal

- Placa: HW-364A
- MCU: ESP8266
- Lógica: 3.3 V
- Limitación crítica: GPIO reducidos

### 3.2 Sistema de Energía

- Tierra común (GND) para todo el sistema
- Motor alimentado por fuente externa independiente
- Electrónica alimentada a 3.3 V

### 3.3 Asignación de Pines

| Función | Señal | Pin |
|------|------|-----|
| OLED SDA | Datos I2C | D2 |
| OLED SCL | Reloj I2C | D1 |
| Encoder A | Giro A | D5 |
| Encoder B | Giro B | D6 |
| Botón Centro | Start | D7 |
| Botón Back | Reversa | D8 |
| Botón Confirm | Adelante | D3 |
| Driver IN1 | Motor A | D0 |
| Driver IN2 | Motor B | D4 |

### 3.4 Driver de Potencia

- Modelo: DRV8871
- Control PWM bidireccional
- Polaridad definida por IN1 / IN2

---

## 4. Lógica de Control

### 4.1 Máquina de Estados

| Estado | Descripción |
|------|-------------|
| WAITING_START | Sistema armado, motor bloqueado |
| SELECT_DIR | Selección explícita de sentido |
| RUNNING | Operación normal |
| CHANGING_DIR | Inversión controlada |

### 4.2 Arranque Seguro

El motor permanece apagado hasta confirmación explícita del usuario. Previene reinicios peligrosos tras fallas de energía.

### 4.3 Selección de Sentido

- BACK → Reversa
- CONFIRM → Adelante

No existe sentido por defecto.

### 4.4 Control de Velocidad Adaptativo

- Giro lento → ±1 %
- Giro rápido (< 50 ms) → ±5 %
- Rango: 0–100 %

### 4.5 Rampa de Aceleración

Variables desacopladas:

- `targetSpeed` → intención del usuario
- `currentSpeed` → acción física

Incremento/decremento: 0.5 % por ciclo (~2.5 s a plena escala).

### 4.6 Inversión Inteligente

Secuencia obligatoria:

1. Rampa descendente hasta 0 %
2. Pausa mecánica
3. Cambio de polaridad
4. Rampa ascendente

---

## 5. Firmware

### 5.1 Inicialización (`setup()`)

- Configuración de GPIO
- Motor apagado por defecto
- Botones con pull-up interno
- Inicialización OLED

### 5.2 Bucle Principal (`loop()`)

- Lectura del encoder
- Ejecución de máquina de estados
- Control de rampa
- Render de interfaz

### 5.3 Funciones Críticas

- `readEncoderSpeed()` → velocidad adaptativa
- `handleDirectionButtons()` → solicitud de inversión
- `updateMotorRamp()` → núcleo de control
- `drawInterface()` → visualización

---

## 6. Interfaz de Usuario

- Flechas grandes indican sentido
- Porcentaje central = velocidad real
- Indicador secundario = velocidad objetivo

Diseño orientado a lectura inmediata.

---

## 7. Limitaciones Actuales

- GPIO al límite
- Sin indicadores periféricos
- Expansión restringida

---

## 8. Versión 2 (V2)

### 8.1 Migración a ESP32

- Mayor número de GPIO
- PWM por hardware
- Escalabilidad

### 8.2 LED RGB de Estado

| Color | Significado |
|------|-------------|
| Amarillo | Standby / 0 % |
| Verde | Adelante |
| Rojo | Reversa |
| Rojo/Verde | Inversión |

### 8.3 Cambios en Firmware

- Nueva función `updateLeds()`
- GPIO dedicados (25, 26, 27)
- Mezcla de color por PWM

---

## 9. Cargar el Firmware con Arduino CLI

### 9.1 Requisitos Previos

```bash
# Instalar Arduino CLI (Linux/macOS)
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh

# O mediante package manager
brew install arduino-cli  # macOS
sudo apt install arduino-cli  # Debian/Ubuntu
```

### 9.2 Configuración Inicial

```bash
# Configurar directorio de datos
export ArduinoDataDir="$HOME/.arduino15"

# Instalar la plataforma ESP8266
arduino-cli core install esp8266:esp8266

# Instalar bibliotecas requeridas
arduino-cli lib install "Adafruit GFX Library"
arduino-cli lib install "Adafruit SSD1306"
```

### 9.3 Compilar

```bash
arduino-cli compile -b esp8266:esp8266:nodemcu drill_vanity.ino
```

### 9.4 Cargar a la Placa

```bash
# Listar puertos disponibles
arduino-cli board list

# Cargar el firmware (reemplazar /dev/ttyUSB0 con tu puerto)
arduino-cli upload -b esp8266:esp8266:nodemcu -p /dev/ttyUSB0 drill_vanity.ino
```

### 9.5 Comando Completo (Una Línea)

```bash
arduino-cli compile -b esp8266:esp8266:nodemcu -p /dev/ttyUSB0 --upload drill_vanity.ino
```

---

## 10. Conclusión

El sistema establece una base sólida para control seguro y profesional de motores DC, con una arquitectura preparada para evolucionar hacia una solución embebida más robusta en ESP32.
