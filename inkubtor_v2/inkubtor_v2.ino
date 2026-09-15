// ====================================================================
// ПОЛНЫЙ КОД УМНОГО ИНКУБАТОРА
// Объединяет: GyverOLED, DHT11, Шаговый двигатель и Нагреватель
// ====================================================================

#include <GyverOLED.h>
#include <DHT.h>

// ====================================================================
// 1. НАСТРОЙКИ ПИНОВ
// ====================================================================
const int PIN_DHT = 2;             // Датчик DHT11
const int PIN_HEATER = 8;          // Нагреватель (реле или лампочка через транзистор)
const int PIN_IN1 = 4;             // Шаговый двигатель (ULN2003)
const int PIN_IN2 = 5;
const int PIN_IN3 = 6;
const int PIN_IN4 = 7;

// ====================================================================
// 2. НАСТРОЙКИ ИНКУБАТОРА (МОЖНО МЕНЯТЬ)
// ====================================================================
#define DHT_TYPE DHT11             // Тип датчика (DHT11 или DHT22)

const float TARGET_TEMP = 37.5;    // Целевая температура, °C
const float HYSTERESIS = 0.3;      // Гистерезис нагрева (вкл при 37.2, выкл при 37.5)

// Настройки поворота яиц
const uint16_t STEPS_PER_TURN = 500*18; // Сколько шагов делает мотор за один поворот (подберите экспериментально)
const unsigned long STEP_DELAY = 2;  // Задержка между шагами в мс (2 мс = быстро и с хорошим усилием)

// ВНИМАНИЕ: Для реального инкубатора поставьте 4 часа: 
// const unsigned long TURN_INTERVAL = 4UL * 60UL * 60UL * 1000UL; 
const unsigned long TURN_INTERVAL = 5000; // <-- ПОКА 5 СЕКУНД ДЛЯ ТЕСТА!

// ====================================================================
// 3. МАССИВ ШАГОВ ДВИГАТЕЛЯ (Ваш 7-шаговый вариант)
// ====================================================================
const byte steps[7][4] = {
  {LOW,  LOW,  HIGH, HIGH}, // Шаг 0
  {LOW,  LOW,  HIGH, LOW }, // Шаг 1
  {LOW,  HIGH, HIGH, LOW }, // Шаг 2
  {HIGH, HIGH, LOW,  LOW }, // Шаг 3
  {HIGH, LOW,  LOW,  LOW }, // Шаг 4
  {HIGH, LOW,  LOW,  HIGH}, // Шаг 5
  {LOW,  LOW,  LOW,  HIGH}  // Шаг 6
};

// ====================================================================
// 4. ОБЪЕКТЫ И ПЕРЕМЕННЫЕ
// ====================================================================
GyverOLED<SSD1306_128x64, OLED_NO_BUFFER> oled;
DHT dht(PIN_DHT, DHT_TYPE);

float temperature = 0.0;
float humidity = 0.0;
bool sensorReady = false;

// Переменные для мотора
bool isTurning = false;
uint16_t currentStep = 0;
bool isClockwise = true;       // Текущее направление
unsigned long lastStepTime = 0;
unsigned long lastTurnTime = 0;

// Переменные для таймеров (неблокирующая работа)
unsigned long lastDhtTime = 0;
unsigned long lastDisplayTime = 0;

// ====================================================================
// 5. SETUP (Инициализация)
// ====================================================================
void setup() {
  Serial.begin(9600);
  
  // Настройка пинов
  pinMode(PIN_HEATER, OUTPUT);
  pinMode(PIN_IN1, OUTPUT);
  pinMode(PIN_IN2, OUTPUT);
  pinMode(PIN_IN3, OUTPUT);
  pinMode(PIN_IN4, OUTPUT);
  
  stopMotor(); // Гарантированно выключаем мотор при старте
  
  dht.begin();
  
  // Приветствие на OLED
  oled.init();
  oled.clear();
  oled.setScale(3);
  oled.home();
  oled.print("Привет!");
  oled.update();
  delay(1000);
  
  oled.clear();
  oled.setScale(1);
  oled.setCursor(0, 3);
  oled.print("ЭТО умный инкубатор");
  oled.update();
  delay(1500);
  
  lastTurnTime = millis();
  Serial.println("Система запущена");
}

// ====================================================================
// 6. LOOP (Главный цикл)
// ====================================================================
void loop() {
  unsigned long currentTime = millis();

  // 1. Опрос датчика каждые 2 секунды
  if (currentTime - lastDhtTime >= 2000) {
    readSensors();
    lastDhtTime = currentTime;
  }

  // 2. Управление нагревателем (простой термостат)
  controlHeater();

  // 3. Логика поворота яиц (неблокирующая)
  handleMotor(currentTime);

  // 4. Обновление экрана каждые 500 мс
  if (currentTime - lastDisplayTime >= 500) {
    updateDisplay();
    lastDisplayTime = currentTime;
  }
}

// ====================================================================
// 7. ФУНКЦИИ
// ====================================================================

// Чтение датчика DHT
void readSensors() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (!isnan(h) && !isnan(t)) {
    humidity = h;
    temperature = t;
    sensorReady = true;
  } else {
    sensorReady = false;
    Serial.println("Ошибка чтения DHT11");
  }
}

// Управление нагревателем (гистерезис)
void controlHeater() {
  if (!sensorReady) return;
  
  if (temperature < (TARGET_TEMP - HYSTERESIS)) {
    digitalWrite(PIN_HEATER, HIGH); // Включить нагрев
  } else if (temperature >= TARGET_TEMP) {
    digitalWrite(PIN_HEATER, LOW);  // Выключить нагрев
  }
}

// Логика шагового двигателя
void handleMotor(unsigned long currentTime) {
  // Если сейчас идет поворот
  if (isTurning) {
    if (currentTime - lastStepTime >= STEP_DELAY) {
      // Выбираем индекс шага: прямой или обратный (математический реверс)
      int stepIndex = currentStep % 7;
      if (!isClockwise) {
        stepIndex = (6 - stepIndex); // Реверс для 7-шагового массива
      }

      // Подаем напряжение на пины
      digitalWrite(PIN_IN1, steps[stepIndex][0]);
      digitalWrite(PIN_IN2, steps[stepIndex][1]);
      digitalWrite(PIN_IN3, steps[stepIndex][2]);
      digitalWrite(PIN_IN4, steps[stepIndex][3]);

      currentStep++;
      lastStepTime = currentTime;

      // Проверка: закончили ли мы нужный поворот?
      if (currentStep >= STEPS_PER_TURN) {
        stopMotor();
        isTurning = false;
        isClockwise = !isClockwise; // Меняем направление на следующий раз!
        lastTurnTime = currentTime; // Засекаем время для следующего интервала
        Serial.println("Поворот завершен. Следующий раз будет в обратную сторону.");
      }
    }
  } 
  // Если не крутимся, проверяем, не пора ли начать
  else {
    if (currentTime - lastTurnTime >= TURN_INTERVAL) {
      Serial.println("Начинаем поворот яиц...");
      isTurning = true;
      currentStep = 0;
      lastStepTime = currentTime;
    }
  }
}

// Остановка мотора (снятие напряжения с катушек для экономии и охлаждения драйвера)
void stopMotor() {
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, LOW);
  digitalWrite(PIN_IN3, LOW);
  digitalWrite(PIN_IN4, LOW);
}

// Отрисовка интерфейса на OLED
void updateDisplay() {
  oled.clear();
  
  // Верхняя строка: Температура и Влажность
  oled.setScale(2);
  oled.setCursor(0, 0);
  if (sensorReady) {
    oled.print(temperature, 1);
    oled.print("°C ");
    oled.print(humidity, 0);
    oled.print("%");
  } else {
    oled.print("--.-°C --%");
  }
  
  // Средняя часть: Статус нагрева
  oled.setScale(1);
  oled.setCursor(0, 3);
  if (digitalRead(PIN_HEATER) == HIGH) {
    oled.print("[HEATER: ON ]");
  } else {
    oled.print("[HEATER: OFF]");
  }
  
  // Нижняя часть: Статус поворота
  oled.setCursor(0, 6);
  if (isTurning) {
    oled.print(isClockwise ? ">>> ПОВОРОТ ВПРАВО" : "<<< ПОВОРОТ ВЛЕВО");
  } else {
    unsigned long minsLeft = (TURN_INTERVAL - (millis() - lastTurnTime)) / 60000;
    oled.print("След. поворот: ");
    oled.print(minsLeft);
    oled.print(" мин");
  }
  
  oled.update();
}