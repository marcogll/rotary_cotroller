/*
 * Proyecto: Control de Motor DC - Consola Digital v1.0
 * Archivo: drill_vanity.ino
 * Autor: Marco Gallegos
 * Hardware: ESP8266 (HW-364A), DRV8871, OLED SSD1306, Encoder
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- DEFINICIÓN DE PINES ---
// Nota: Usamos la notación Dx del NodeMCU para claridad, mapeada a GPIO
#define PIN_CON     0   // D3 (GPIO0) - Adelante / Confirmar (Active LOW)
#define PIN_SDA     4   // D2 (GPIO4) - I2C Data
#define PIN_SCL     5   // D1 (GPIO5) - I2C Clock
#define PIN_PSH     13  // D7 (GPIO13) - Botón Encoder (Start/Stop)
#define PIN_RTA     14  // D5 (GPIO14) - Encoder A
#define PIN_TRB     12  // D6 (GPIO12) - Encoder B
#define PIN_BAK     15  // D8 (GPIO15) - Atrás / Reversa (Active LOW)
#define PIN_IN1     16  // D0 (GPIO16) - PWM A
#define PIN_IN2     2   // D4 (GPIO2)  - PWM B

// --- CONFIGURACIÓN OLED ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- MÁQUINA DE ESTADOS ---
enum SystemState {
  WAITING_START,  // Sistema armado, motor bloqueado
  SELECT_DIR,     // Esperando selección de sentido
  RUNNING,        // Motor operando
  CHANGING_DIR    // Transición de seguridad
};

SystemState currentState = WAITING_START;
bool directionForward = true; // true = Adelante, false = Atrás

// --- VARIABLES DE CONTROL MOTOR ---
int targetSpeed = 0;       // Velocidad deseada (0-255)
float currentSpeed = 0;    // Velocidad real (Rampa)
const float RAMP_STEP = 0.8; // Ajuste de suavidad (menor = más suave)
unsigned long lastRampTime = 0;
const int RAMP_INTERVAL = 5; // ms

// --- VARIABLES ENCODER ---
volatile int encoderValue = 0;
volatile unsigned long lastInterruptTime = 0;
unsigned long lastEncoderChangeTime = 0; 

// --- DEBOUNCE BOTONES ---
unsigned long lastDebounceTime = 0;
const int DEBOUNCE_DELAY = 200;

// --- ISR (Interrupción Encoder) ---
ICACHE_RAM_ATTR void handleEncoder() {
  unsigned long interruptTime = millis();
  if (interruptTime - lastInterruptTime > 5) { 
    if (digitalRead(PIN_RTA) == digitalRead(PIN_TRB)) {
      encoderValue--;
    } else {
      encoderValue++;
    }
    lastInterruptTime = interruptTime;
  }
}

void setup() {
  Serial.begin(115200);

  // Configuración de Pines
  // D3 y D8 son pines de BOOT. Se configuran INPUT_PULLUP en setup,
  // pero el hardware debe asegurar sus estados correctos al encender.
  pinMode(PIN_CON, INPUT_PULLUP);
  pinMode(PIN_BAK, INPUT_PULLUP); 
  pinMode(PIN_PSH, INPUT_PULLUP);
  
  pinMode(PIN_RTA, INPUT_PULLUP);
  pinMode(PIN_TRB, INPUT_PULLUP);
  
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  
  // Motor apagado al inicio
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, LOW);

  attachInterrupt(digitalPinToInterrupt(PIN_RTA), handleEncoder, CHANGE);

  // Inicializar OLED
  Wire.begin(PIN_SDA, PIN_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("Error OLED"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.display();
}

void loop() {
  handleStateMachine();
  updateMotorOutput();
  updateDisplay();
  delay(10); 
}

void handleStateMachine() {
  unsigned long now = millis();

  bool btnCon = (digitalRead(PIN_CON) == LOW);
  bool btnBak = (digitalRead(PIN_BAK) == LOW);
  bool btnPsh = (digitalRead(PIN_PSH) == LOW);

  // Filtro de rebotes
  if ((btnCon || btnBak || btnPsh) && (now - lastDebounceTime < DEBOUNCE_DELAY)) return;
  if (btnCon || btnBak || btnPsh) lastDebounceTime = now;

  switch (currentState) {
    case WAITING_START:
      targetSpeed = 0;
      currentSpeed = 0;
      if (btnPsh) currentState = SELECT_DIR;
      break;

    case SELECT_DIR:
      if (btnCon) {
        directionForward = true;
        targetSpeed = 60; // Arranca suave
        currentState = RUNNING;
      } else if (btnBak) {
        directionForward = false;
        targetSpeed = 60;
        currentState = RUNNING;
      } else if (btnPsh) {
        currentState = WAITING_START; // Cancelar
      }
      break;

    case RUNNING:
      // Control de velocidad con encoder
      if (encoderValue != 0) {
        int change = encoderValue;
        // Aceleración adaptativa según velocidad de giro del encoder
        int step = (millis() - lastEncoderChangeTime < 50) ? 10 : 2;
        
        targetSpeed += (change * step);
        targetSpeed = constrain(targetSpeed, 0, 255);
        encoderValue = 0;
        lastEncoderChangeTime = now;
      }

      // Parada
      if (btnPsh) {
        targetSpeed = 0;
        if (currentSpeed < 10) currentState = WAITING_START;
      }
      
      // Cambio de sentido "al vuelo"
      if ((directionForward && btnBak) || (!directionForward && btnCon)) {
        currentState = CHANGING_DIR;
      }
      break;

    case CHANGING_DIR:
      targetSpeed = 0;
      // Esperar a que el motor se detenga casi por completo
      if (currentSpeed <= 5) {
        delay(300); // Pequeña pausa mecánica
        directionForward = !directionForward;
        currentState = RUNNING;
        targetSpeed = 60; 
      }
      break;
  }
}

void updateMotorOutput() {
  unsigned long now = millis();
  
  // Lógica de Rampa
  if (now - lastRampTime >= RAMP_INTERVAL) {
    if (currentSpeed < targetSpeed) {
      currentSpeed += RAMP_STEP;
      if (currentSpeed > targetSpeed) currentSpeed = targetSpeed;
    } else if (currentSpeed > targetSpeed) {
      currentSpeed -= RAMP_STEP;
      if (currentSpeed < targetSpeed) currentSpeed = targetSpeed;
    }
    lastRampTime = now;
  }

  int pwmOutput = (int)currentSpeed;

  if (currentState == WAITING_START) {
    digitalWrite(PIN_IN1, LOW);
    digitalWrite(PIN_IN2, LOW);
  } else {
    // DRV8871 Control
    if (directionForward) {
      analogWrite(PIN_IN1, pwmOutput);
      digitalWrite(PIN_IN2, LOW);
    } else {
      digitalWrite(PIN_IN1, LOW);
      analogWrite(PIN_IN2, pwmOutput);
    }
  }
}

void updateDisplay() {
  static unsigned long lastDisp = 0;
  if (millis() - lastDisp < 100) return;
  lastDisp = millis();

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  
  switch (currentState) {
    case WAITING_START:
      display.println(F(">> SISTEMA LISTO <<"));
      display.setCursor(25, 25);
      display.setTextSize(2);
      display.print(F("PULSAR"));
      display.setCursor(35, 45);
      display.print(F("START"));
      break;

    case SELECT_DIR:
      display.println(F(" SELECCIONAR GIRO"));
      display.setCursor(10, 30);
      display.setTextSize(2);
      display.print(F("<< O >>"));
      break;

    case RUNNING:
    case CHANGING_DIR:
      display.print(currentState == CHANGING_DIR ? F("! INVIRTIENDO !") : (directionForward ? F("GIRO: ADELANTE >") : F("GIRO: < ATRAS")));
      
      display.setTextSize(3);
      int pct = map((int)currentSpeed, 0, 255, 0, 100);
      display.setCursor(35, 20);
      display.print(pct);
      display.setTextSize(2);
      display.print(F("%"));
      
      display.drawRect(5, 58, 118, 6, SSD1306_WHITE);
      display.fillRect(7, 60, map(pct,0,100,0,114), 2, SSD1306_WHITE);
      break;
  }
  display.display();
}