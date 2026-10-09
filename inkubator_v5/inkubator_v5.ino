/**
 * Умный Инкубатор v1.1
 * 
 * Объединяет: DHT11 (климат), Реле (нагрев/увлажнение), 
 * Шаговый двигатель (лоток 1), Сервопривод (лоток 2) и OLED-дисплей.
 */

#include <GyverOLED.h>
#include <DHT.h>
#include <Servo.h>          // <-- ДОБАВЛЕНО: Библиотека для сервопривода

// --- Конфигурация пинов ---
#define DHT_PIN 2
#define RELAY_PIN 12
#define SERVO_PIN 8         // <-- ДОБАВЛЕНО: Сигнальный пин сервопривода
#define IN1 6
#define IN2 5
#define IN3 4
#define IN4 3

// --- Настройки инкубатора ---
const float TARGET_TEMP = 37.5;       // Целевая температура (°C)
const float TARGET_HUMIDITY = 60.0;   // Целевая влажность (%)
const float HYSTERESIS = 0.5;         // Гистерезис реле (вкл. при T < 37.0, выкл. при T >= 37.5)

const unsigned long TURN_INTERVAL = 30000UL; // Интервал переворота (30 сек для теста, 7200000UL для 2 часов)

// Настройки шагового двигателя (лоток 1)
const int STEP_DELAY = 1;          
const int STEPS_PER_TURN = 1500;   

// Настройки сервопривода (лоток 2)
const int SERVO_ANGLE_LEFT = 45;    // Угол наклона влево
const int SERVO_ANGLE_RIGHT = 135;  // Угол наклона вправо (90 + 45)
const int SERVO_SPEED = 15;         // Задержка в мс между градусами (чем больше, тем плавнее)

// --- Инициализация объектов ---
GyverOLED<SSD1306_128x64, OLED_NO_BUFFER> oled;
DHT dht(DHT_PIN, DHT11);
Servo trayServo;          // <-- ДОБАВЛЕНО: Объект сервопривода

// --- Переменные состояния ---
float currentTemp = 0.0;
float currentHumidity = 0.0;
bool relayState = false;
int currentServoAngle = SERVO_ANGLE_LEFT; // Текущий угол сервопривода

unsigned long lastDHTRead = 0;
unsigned long lastTurnTime = 0;

// --- Последовательность шагов для шагового двигателя ---
const byte stepSequence[7][4] = {
  {LOW,  LOW,  HIGH, HIGH},
  {LOW,  LOW,  HIGH, LOW},
  {LOW,  HIGH, HIGH, LOW},
  {HIGH, HIGH, LOW,  LOW},
  {HIGH, LOW,  LOW,  LOW},
  {HIGH, LOW,  LOW,  HIGH},
  {LOW,  LOW,  LOW,  HIGH}
};

void setup() {
  Serial.begin(9600);
  
  // 1. Инициализация датчика
  dht.begin();
  
  // 2. Инициализация реле
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); 
  
  // 3. Инициализация шагового двигателя
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  stopStepper();
  
  // 4. Инициализация сервопривода
  trayServo.attach(SERVO_PIN);
  trayServo.write(currentServoAngle); // Устанавливаем начальное положение
  delay(500); // Даем сервоприводу время занять позицию при старте
  
  // 5. Инициализация OLED дисплея
  oled.init();
  oled.clear();
  oled.setScale(2);
  oled.setCursor(0, 1);
  oled.print("УМНЫЙ");
  oled.setCursor(0, 3);
  oled.print("ИНКУБАТОР");
  delay(2000);
  oled.clear();
}

void loop() {
  unsigned long currentMillis = millis();

  // --- Блок 1: Чтение датчика и управление реле (каждые 2 секунды) ---
  if (currentMillis - lastDHTRead >= 2000) {
    lastDHTRead = currentMillis;
    
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (!isnan(h) && !isnan(t)) {
      currentHumidity = h;
      currentTemp = t;
      
      if (currentTemp < TARGET_TEMP - HYSTERESIS) {
        relayState = true;
      } else if (currentTemp >= TARGET_TEMP) {
        relayState = false;
      }
      
      digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);
    }
  }

  // --- Блок 2: Переворот яиц по расписанию ---
  if (currentMillis - lastTurnTime >= TURN_INTERVAL) {
    lastTurnTime = currentMillis;
    
    turnStepperTray();   // Поворот первого лотка (шаговый двигатель)
    tiltServoTray();     // Качение второго лотка (сервопривод)
  }

  // --- Блок 3: Обновление OLED дисплея ---
  updateDisplay();
  
  delay(50);
}

// --- Функция переворота первого лотка (Шаговый двигатель) ---
void turnStepperTray() {
  Serial.println(">> Выполняется переворот 1-го лотка (шаговый)...");
  
  for (int cycle = 0; cycle < STEPS_PER_TURN; cycle++) {
    for (int i = 0; i < 7; i++) {
      digitalWrite(IN1, stepSequence[i][0]);
      digitalWrite(IN2, stepSequence[i][1]);
      digitalWrite(IN3, stepSequence[i][2]);
      digitalWrite(IN4, stepSequence[i][3]);
      delay(STEP_DELAY);
    }
  }
  stopStepper();
  Serial.println(">> Переворот 1-го лотка завершен.");
}

// --- Функция качения второго лотка (Сервопривод) ---
void tiltServoTray() {
  Serial.println(">> Начинаем качение 2-го лотка (сервопривод)...");
  
  // Переключаем целевой угол: если был 45, станет 135. Если 135, станет 45.
  if (currentServoAngle == SERVO_ANGLE_LEFT) {
    currentServoAngle = SERVO_ANGLE_RIGHT;
  } else {
    currentServoAngle = SERVO_ANGLE_LEFT;
  }

  // Получаем текущий реальный угол сервопривода
  int startAngle = trayServo.read();
  
  // Плавное движение к целевому углу
  if (startAngle < currentServoAngle) {
    for (int pos = startAngle; pos <= currentServoAngle; pos += 1) {
      trayServo.write(pos);
      delay(SERVO_SPEED); // Плавность движения
    }
  } else {
    for (int pos = startAngle; pos >= currentServoAngle; pos -= 1) {
      trayServo.write(pos);
      delay(SERVO_SPEED); // Плавность движения
    }
  }
  
  Serial.print(">> Качение 2-го лотка завершено. Текущий угол: ");
  Serial.println(currentServoAngle);
}

// --- Функция полной остановки шагового двигателя ---
void stopStepper() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

// --- Функция отрисовки интерфейса на OLED ---
void updateDisplay() {
  oled.clear();
  oled.setScale(1);
  
  oled.setCursor(0, 0);
  oled.print("T: ");
  oled.print(currentTemp, 1);
  oled.print(" C ");
  oled.print(relayState ? "[РЕЛЕ ON ]" : "[РЕЛЕ OFF]");

  oled.setCursor(0, 2);
  oled.print("H: ");
  oled.print(currentHumidity, 1);
  oled.print(" % (ЦЕЛЬ: ");
  oled.print(TARGET_HUMIDITY, 0);
  oled.print(")");

  oled.setCursor(0, 4);
  unsigned long timeToTurn = (TURN_INTERVAL - (millis() - lastTurnTime)) / 1000UL;
  oled.print("След. переворот: ");
  oled.print(timeToTurn);
  oled.print(" s");

  oled.setCursor(0, 6);
  oled.print("Угол серво: ");
  oled.print(currentServoAngle);
  oled.print(" deg      "); // Пробелы для затирания старых символов
}