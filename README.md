Proyecto de Control de Motor DC: Explicación Integral
Este proyecto consiste en crear una "consola de mando" digital para un motor de corriente continua. En lugar de usar un potenciómetro simple que cambia la velocidad de golpe, estás construyendo un sistema inteligente que gestiona la aceleración suave, protege el motor en los cambios de giro y te da retroalimentación visual.
1. El Hardware y el Pinout (La base física)
Estamos utilizando la placa HW-364A (ESP8266). Esta placa es el "cerebro". El desafío principal con ella es que tiene pocos pines disponibles, y tu proyecto requiere conectar muchos periféricos.
Diagrama de Conexiones en tu Protoboard
Imagina el flujo de información: Tu mano mueve el encoder -> El ESP8266 procesa la señal -> El ESP8266 manda instrucciones al Driver -> El Driver da energía al Motor -> La Pantalla te muestra qué pasó.
Aquí está la asignación física de pines que debes tener en tu protoboard:
Sistema de Energía:
Todo comparte la tierra (GND).
El Motor recibe su voltaje de una fuente externa (Baterías o Fuente de Poder), NO del USB.
La electrónica (Pantalla, Encoder, ESP) se alimenta con 3.3V.
La Pantalla y el Encoder (Módulo Integrado):
SDA (Datos) -> D2: Por aquí viaja la información para dibujar en la pantalla.
SCL (Reloj) -> D1: Sincroniza la comunicación de la pantalla.
A (Encoder) -> D5: Detecta el giro hacia un lado.
B (Encoder) -> D6: Detecta el giro hacia el otro.
Btn Centro -> D7: Es el "Enter" o arranque.
Btn Back -> D8: Botón para ir hacia atrás (Reversa).
Btn Confirm -> D3: Botón para ir hacia adelante.
El Músculo (Driver DRV8871):
IN1 -> D0: Controla la potencia en una polaridad.
IN2 -> D4: Controla la potencia en la polaridad opuesta.
2. Lógica de Funcionamiento (El Comportamiento)
El programa no solo "gira y ya". Tiene una máquina de estados que define su comportamiento seguro:
A. Estado de Inicio (Seguridad)
Cuando enciendes el sistema, el motor no arranca. La pantalla te pide presionar el botón central. Esto evita que, si se fue la luz y vuelve, el motor arranque solo y cause un accidente.
B. Selección de Sentido
Una vez activado, el sistema te pregunta: ¿Hacia dónde vamos? Debes presionar el botón de la izquierda (BACK) para Reversa o el de la derecha (CONFIRM) para Adelante.
C. Control de Velocidad (El Encoder)
Aquí hay una lógica interesante llamada "Velocidad Adaptativa":
Giro Fino: Si giras la perilla despacio, la velocidad sube de 1% en 1%. Ideal para ajustes precisos.
Giro Rápido: Si giras la perilla rápidamente (menos de 50ms entre clicks), el sistema detecta la urgencia y sube la velocidad de 5% en 5%.
D. La Rampa de Aceleración (Física Suave)
Si estás al 0% y pides ir al 100%, el motor no salta instantáneamente.
El código tiene una variable targetSpeed (lo que tú quieres) y una variable currentSpeed (lo que el motor está haciendo). El programa ajusta la currentSpeed poco a poco hasta alcanzar a la targetSpeed. Esto toma 2.5 segundos. Esto protege los engranajes del motor y evita picos de corriente.
E. Cambio de Sentido (Inversión Inteligente)
Si el motor va hacia adelante al 100% y presionas "Atrás", el sistema hace esto automáticamente:
Baja la velocidad suavemente de 100% a 0% (Rampa de desaceleración).
Espera un momento (zona muerta) para asegurar que el eje se detuvo.
Cambia la polaridad eléctrica interna.
Sube la velocidad suavemente de 0% a 100% en el nuevo sentido.
3. Explicación de las Funciones del Código
Aquí te explico qué hace cada parte del programa que verás más abajo:
setup(): Prepara los pines. Define cuáles son entradas (botones) y salidas (motor). Inicia la pantalla OLED y muestra el mensaje de bienvenida.
loop(): Es el cerebro principal que se repite infinitamente. Usa un switch (systemState) para saber si está esperando, eligiendo dirección o corriendo.
readEncoderSpeed(): Lee el encoder. Calcula el tiempo entre giros para decidir si suma 1 o 5 a la velocidad.
handleDirectionButtons(): Vigila si presionas BACK o CONFIRM. Si lo haces, cambia el estado del sistema a CHANGING_DIR.
updateMotorRamp(): Esta es la función más importante. Compara la velocidad real con la deseada. Si son diferentes, suma o resta una pequeña fracción (0.5) en cada ciclo. Luego convierte ese número en una señal eléctrica (PWM) para el motor.
drawInterface(): Dibuja en la pantalla OLED las flechas (<- o ->), el número de porcentaje grande y el objetivo pequeño.
4. El Código (Versión 1 - Hardware Actual)
Copia y pega esto en tu Arduino IDE. Está diseñado para ser estable en tu HW-364A.
code
C++
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH1106.h> // Si la pantalla se ve con lluvia, cambia a Adafruit_SSD1306
#include <RotaryEncoder.h>

// --- ASIGNACIÓN DE PINES (HW-364A) ---
#define PIN_IN1 D0  // Salida Motor A
#define PIN_IN2 D4  // Salida Motor B
#define PIN_SDA D2  // Pantalla
#define PIN_SCL D1  // Pantalla

#define PIN_ENC_A D5       // Encoder Giro A
#define PIN_ENC_B D6       // Encoder Giro B
#define PIN_BTN_CENTER D7  // Botón encoder
#define PIN_BTN_BACK D8    // Botón Izquierdo
#define PIN_BTN_CONFIRM D3 // Botón Derecho

// --- OBJETOS ---
#define OLED_RESET -1
Adafruit_SH1106 display(OLED_RESET); 
// Nota: Si usas SSD1306 descomenta la línea de abajo y comenta la de arriba:
// #include <Adafruit_SSD1306.h>
// Adafruit_SSD1306 display(128, 64, &Wire, OLED_RESET);

// Configuración del encoder para evitar rebotes
RotaryEncoder encoder(PIN_ENC_A, PIN_ENC_B, RotaryEncoder::LatchMode::TWO03);

// --- VARIABLES GLOBALES ---
int targetSpeed = 0;       // La velocidad que TÚ quieres
float currentSpeed = 0;    // La velocidad que el MOTOR tiene ahora (float para suavidad)
int lastEncoded = 0;
unsigned long lastEncoderTime = 0;

// Máquina de Estados
enum State { WAITING_START, SELECT_DIR, RUNNING, CHANGING_DIR };
State systemState = WAITING_START;

enum Direction { FWD, REV };
Direction currentDir = FWD;
Direction nextDir = FWD;

// --- CONFIGURACIÓN INICIAL ---
void setup() {
  Serial.begin(115200); // Para depurar errores en PC

  // Configurar Motor (Apagado al inicio)
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, LOW);

  // Configurar Botones con resistencia interna (Pullup)
  pinMode(PIN_BTN_CENTER, INPUT_PULLUP);
  pinMode(PIN_BTN_BACK, INPUT_PULLUP);
  pinMode(PIN_BTN_CONFIRM, INPUT_PULLUP);

  // Iniciar Pantalla
  display.begin(SH1106_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(10, 20);
  display.println("INICIANDO SISTEMA...");
  display.display();
  delay(1000);
}

// --- BUCLE PRINCIPAL ---
void loop() {
  encoder.tick(); // Revisar si el encoder se movió

  switch (systemState) {
    // 1. ESTADO DE ESPERA
    case WAITING_START:
      displayCentered("PRESIONA CENTRO", "PARA ACTIVAR");
      if (digitalRead(PIN_BTN_CENTER) == LOW) { // Al presionar (LOW)
        delay(300); // Pequeña espera para evitar doble click
        systemState = SELECT_DIR;
      }
      break;

    // 2. ESTADO DE SELECCIÓN
    case SELECT_DIR:
      display.clearDisplay();
      display.setCursor(0,0); display.println("ELIGE SENTIDO:");
      display.setCursor(0,30); display.println("<- BACK (REV)");
      display.setCursor(0,50); display.println("CONFIRM (FWD) ->");
      display.display();

      if (digitalRead(PIN_BTN_BACK) == LOW) {
        currentDir = REV;
        systemState = RUNNING;
        delay(300);
      }
      if (digitalRead(PIN_BTN_CONFIRM) == LOW) {
        currentDir = FWD;
        systemState = RUNNING;
        delay(300);
      }
      break;

    // 3. ESTADO DE CARRERA (NORMAL)
    case RUNNING:
      readEncoderSpeed();       // Leer perilla
      handleDirectionButtons(); // Leer botones de cambio
      updateMotorRamp();        // Mover motor suavemente
      drawInterface();          // Dibujar
      break;

    // 4. ESTADO DE CAMBIO DE DIRECCIÓN
    case CHANGING_DIR:
      targetSpeed = 0;   // Ordenar freno total
      updateMotorRamp(); // Ejecutar freno (respetando la rampa)
      drawInterface();
      
      // Cuando la velocidad real llegue casi a 0
      if (currentSpeed <= 1.0) { 
        delay(100); // Pausa de seguridad mecánica
        currentDir = nextDir;      // Cambiar polaridad interna
        targetSpeed = lastEncoded; // Recuperar la velocidad que teníamos
        systemState = RUNNING;     // Volver a correr
      }
      break;
  }
  delay(10); // Estabilidad del procesador
}

// --- FUNCIONES AUXILIARES ---

void readEncoderSpeed() {
  int newPos = encoder.getPosition();
  if (newPos != lastEncoded) {
    int diff = newPos - lastEncoded;
    unsigned long now = millis();
    
    // Si giras muy rápido (<50ms), salta de 5 en 5. Si no, de 1 en 1.
    int step = (now - lastEncoderTime < 50) ? 5 : 1; 
    
    if (diff > 0) targetSpeed += step;
    else targetSpeed -= step;

    // Limitar entre 0 y 100
    targetSpeed = constrain(targetSpeed, 0, 100);
    encoder.setPosition(targetSpeed);
    lastEncoded = targetSpeed;
    lastEncoderTime = now;
  }
}

void handleDirectionButtons() {
  // Si presiono BACK y voy hacia Adelante -> Cambiar
  if (digitalRead(PIN_BTN_BACK) == LOW && currentDir == FWD) {
    nextDir = REV;
    systemState = CHANGING_DIR;
  }
  // Si presiono CONFIRM y voy hacia Atrás -> Cambiar
  if (digitalRead(PIN_BTN_CONFIRM) == LOW && currentDir == REV) {
    nextDir = FWD;
    systemState = CHANGING_DIR;
  }
}

void updateMotorRamp() {
  // Acercar currentSpeed a targetSpeed paso a paso
  if (currentSpeed < targetSpeed) {
    currentSpeed += 0.5; // Acelerar
    if (currentSpeed > targetSpeed) currentSpeed = targetSpeed;
  } else if (currentSpeed > targetSpeed) {
    currentSpeed -= 0.5; // Frenar
    if (currentSpeed < targetSpeed) currentSpeed = targetSpeed;
  }

  // Convertir porcentaje (0-100) a PWM (0-1023)
  int pwmVal = map((int)currentSpeed, 0, 100, 0, 1023);
  
  // Zona muerta: Si es muy poco voltaje, mejor apagar para evitar zumbido
  if (pwmVal < 50) pwmVal = 0; 

  // Aplicar al driver DRV8871
  if (currentDir == FWD) {
    analogWrite(PIN_IN1, pwmVal);
    analogWrite(PIN_IN2, 0);
  } else {
    analogWrite(PIN_IN1, 0);
    analogWrite(PIN_IN2, pwmVal);
  }
}

void drawInterface() {
  display.clearDisplay();
  
  // Dibujar flechas de dirección
  display.setTextSize(2);
  display.setCursor(10, 0);
  if (currentDir == FWD) display.print("FWD ->");
  else display.print("<- REV");

  // Dibujar velocidad gigante
  display.setTextSize(3);
  display.setCursor(35, 30);
  display.print((int)currentSpeed);
  display.setTextSize(1);
  display.print("%");
  
  // Dibujar objetivo pequeño (si estamos en rampa)
  if ((int)currentSpeed != targetSpeed) {
    display.setCursor(0, 55);
    display.print("Rampa: "); display.print(targetSpeed);
  }
  display.display();
}

void displayCentered(String line1, String line2) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.println(line1);
  display.println(line2);
  display.display();
}
5. Visión Futura: La Versión 2 (V2)
Has notado que con la placa HW-364A estamos "ahorcados" de pines. No pudimos poner el LED RGB cómodamente. Para la Versión 2, el plan es migrar el hardware.
El Hardware Recomendado: ESP32 DevKit V1
Esta placa es un poco más larga, pero tiene 30 pines. Con esto podemos conectar todo lo anterior y agregar luces de estado profesionales.
Implementación del LED RGB en V2
La idea es usar un LED RGB para dar información periférica (para que sepas qué pasa sin mirar la pantalla OLED):
Luz Amarilla Fija: El sistema está encendido pero en "Standby" o velocidad 0%.
Luz Verde: El motor gira hacia ADELANTE. La intensidad del verde podría subir con la velocidad.
Luz Roja: El motor gira en REVERSA.
Parpadeo Rojo/Verde: El sistema está invirtiendo el giro automáticamente (fase de peligro/atención).
Cambios en Código para V2
En el futuro, solo tendrás que agregar una función llamada updateLeds() dentro del loop(). Al tener pines de sobra (como GPIO 25, 26, 27), podrás usar analogWrite en el LED Rojo y Verde para mezclar colores (Rojo + Verde = Amarillo) y crear una interfaz mucho más profesional.