/**
 * Умный Инкубатор v1.0
 * 
 * Объединяет: DHT11 (климат), Реле (нагрев/увлажнение), 
 * Шаговый двигатель (переворот яиц) и OLED-дисплей (мониторинг).
 * Архитектура неблокирующая (на базе millis), что критично для стабильности.
 */

#include <GyverOLED.h>
#include <DHT.h>

// --- Конфигурация пинов ---
#define DHT_PIN 2
#define RELAY_PIN 12
#define IN1 6
#define IN2 5
#define IN3 4
#define IN4 3

// --- Настройки инкубатора ---
const float TARGET_TEMP = 37.5;       // Целевая температура (°C)
const float TARGET_HUMIDITY = 60.0;   // Целевая влажность (%)
const float HYSTERESIS = 0.5;         // Гистерезис реле (вкл. при T < 37.0, выкл. при T >= 37.5)

// Интервал переворота яиц (для теста 10 секунд, для работы 7200000UL = 2 часа)
const unsigned long TURN_INTERVAL = 30000UL; 

const int STEP_DELAY = 1;          // Задержка между шагами в мс (20-30 мс — оптимально для плавности)
const int STEPS_PER_TURN = 1500;     // Сколько раз повторить последовательность за один переворот
                                    // (150 циклов * 7 шагов = 1050 шагов. Для мотора 28BYJ-48 это ~180 градусов)

// --- Инициализация объектов ---
GyverOLED<SSD1306_128x64, OLED_NO_BUFFER> oled;
DHT dht(DHT_PIN, DHT11);

// --- Переменные состояния ---
float currentTemp = 0.0;
float currentHumidity = 0.0;
bool relayState = false;

unsigned long lastDHTRead = 0;
unsigned long lastTurnTime = 0;

// --- Последовательность шагов (интегрирована из вашего кода) ---
// 7 шагов для управления шаговым двигателем
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
  
  // 2. Инициализация реле (безопасный старт: выключено)
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); 
  
  // 3. Инициализация пинов шагового двигателя
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  stopStepper(); // Снимаем напряжение с катушек для предотвращения перегрева в простое
  
  // 4. Инициализация OLED дисплея
  oled.init();
  oled.clear();
  oled.setScale(2);
  oled.setCursor(0, 1);
  oled.print("УМНЫЙ");
  oled.setCursor(0, 3);
  oled.print("ИНКУБАТОР");
  delay(2000); // Приветственная заставка
  oled.clear();
}

void loop() {
  unsigned long currentMillis = millis();

  // --- Блок 1: Чтение датчика и умное управление реле (каждые 2 секунды) ---
  if (currentMillis - lastDHTRead >= 2000) {
    lastDHTRead = currentMillis;
    
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (!isnan(h) && !isnan(t)) {
      currentHumidity = h;
      currentTemp = t;
      
      // Логика термостата: заменяет опасный blind delay(3000)
      if (currentTemp < TARGET_TEMP - HYSTERESIS) {
        relayState = true;  // Включаем нагрев/увлажнение
      } else if (currentTemp >= TARGET_TEMP) {
        relayState = false; // Выключаем при достижении цели
      }
      
      digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);
    }
  }

  // --- Блок 2: Переворот яиц по расписанию (не блокирует основной цикл) ---
  if (currentMillis - lastTurnTime >= TURN_INTERVAL) {
    lastTurnTime = currentMillis;
    turnEggs();
  }

  // --- Блок 3: Обновление информации на OLED дисплее ---
  updateDisplay();
  
  // Короткая пауза для стабильности работы микроконтроллера
  delay(50);
}

// --- Функция переворота яиц ---
void turnEggs() {
  Serial.println(">> Выполняется переворот яиц...");
  
  // Внешний цикл: количество полных циклов поворота
  for (int cycle = 0; cycle < STEPS_PER_TURN; cycle++) {
    // Внутренний цикл: ваша последовательность из 7 шагов
    for (int i = 0; i < 7; i++) {
      digitalWrite(IN1, stepSequence[i][0]);
      digitalWrite(IN2, stepSequence[i][1]);
      digitalWrite(IN3, stepSequence[i][2]);
      digitalWrite(IN4, stepSequence[i][3]);
      delay(STEP_DELAY);
    }
  }
  
  stopStepper(); // Снимаем напряжение после завершения
  Serial.println(">> Переворот завершен.");
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
  
  // Строка 0: Температура и статус реле
  oled.setCursor(0, 0);
  oled.print("T: ");
  oled.print(currentTemp, 1);
  oled.print(" C ");
  oled.print(relayState ? "[РЕЛЕ ON ]" : "[РЕЛЕ OFF]");

  // Строка 2: Влажность
  oled.setCursor(0, 2);
  oled.print("H: ");
  oled.print(currentHumidity, 1);
  oled.print(" % (ЦЕЛЬ: ");
  oled.print(TARGET_HUMIDITY, 0);
  oled.print(")");

  // Строка 4: Таймер до следующего переворота (ИЗМЕНЕНО: теперь в секундах)
  oled.setCursor(0, 4);
  unsigned long timeToTurn = (TURN_INTERVAL - (millis() - lastTurnTime)) / 1000UL;
  oled.print("След. переворот: ");
  oled.print(timeToTurn);
  oled.print(" s");

  // Строка 6: Системная информация
  oled.setCursor(0, 6);
  oled.print("ВРЕМЯ РАБОТЫ: ");
  oled.print(millis() / 60000UL);
  oled.print(" min");
}