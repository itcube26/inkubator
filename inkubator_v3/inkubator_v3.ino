#include <DHT.h>
#include <GyverOLED.h>

// --- Настройки пинов ---
#define DHT_PIN 2
#define RELAY_PIN 12
#define STEP_IN1 6
#define STEP_IN2 5
#define STEP_IN3 4
#define STEP_IN4 3

// --- Настройки ПД-регулятора ---
float setpoint = 37.5;   // Целевая температура (°C)
float basePower = 20.0;  // Базовая мощность нагрева (%)
float Kp = 10.0;         // Пропорциональный коэффициент
float Kd = 50.0;         // Дифференциальный коэффициент
unsigned long windowSize = 5000; // Окно ШИМ для реле (5000 мс для механического реле)

// --- Настройки переворота яиц ---
// Интервал между переворотами. 3 часа = 10800000 мс.
// ВАЖНО: Для тестирования измените это значение на 10000 (10 секунд)!
const unsigned long TURN_INTERVAL_MS = 10000; 
const int STEPS_PER_TURN = 512;      // Количество шагов на один переворот (зависит от вашего редуктора)
const unsigned long STEP_DELAY_MS = 2; // Задержка между шагами мотора (мс). Меньше = быстрее, но меньше крутящий момент

// --- Инициализация компонентов ---
GyverOLED<SSD1306_128x64, OLED_BUFFER> oled;
DHT dht(DHT_PIN, DHT11);

// --- Переменные для графика ---
const int GRAPH_WIDTH = 128;
int16_t tempHistory[GRAPH_WIDTH]; // Храним температуру * 10

// --- Переменные для ПД-регулятора ---
float last_error = 0;
unsigned long last_time = 0;
unsigned long windowStartTime = 0;
float pdOutputMs = 0;

// --- Переменные для шагового двигателя ---
const int stepPins[4] = {STEP_IN1, STEP_IN2, STEP_IN3, STEP_IN4};
// Стандартная 8-шаговая последовательность (half-step) для драйвера ULN2003
const int stepSequence[8][4] = {
  {1, 0, 0, 0},
  {1, 1, 0, 0},
  {0, 1, 0, 0},
  {0, 1, 1, 0},
  {0, 0, 1, 0},
  {0, 0, 1, 1},
  {0, 0, 0, 1},
  {1, 0, 0, 1}
};

unsigned long lastTurnTime = 0;
int currentStep = 0;
int stepsRemaining = 0;
unsigned long lastStepTime = 0;
bool isTurning = false;

// --- Переменные для DHT ---
float currentTemp = 25.0;
float currentHum = 50.0;
unsigned long lastDHTRead = 0;

// Функция перевода температуры (x10) в координату Y на экране
int getY(int16_t temp_x10) {
  int t = constrain(temp_x10, 300, 450);
  return 63 - ((t - 300) * 39) / 150; // Целочисленная математика для скорости
}

void setup() {
  Serial.begin(9600);
  
  dht.begin();
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  
  // Инициализация пинов мотора
  for (int i = 0; i < 4; i++) {
    pinMode(stepPins[i], OUTPUT);
    digitalWrite(stepPins[i], LOW);
  }
  
  // Приветственное сообщение
  oled.init();
  oled.clear();
  oled.setScale(2);
  oled.setCursorXY(0, 16);
  oled.print("Incubator PD");
  oled.setScale(1);
  oled.setCursorXY(0, 40);
  oled.print("Starting up...");
  oled.update();
  delay(1500);
  
  // Заполняем историю начальной температурой
  float t_init = dht.readTemperature();
  int16_t initialTemp = isnan(t_init) ? 250 : (int16_t)(t_init * 10);
  for (int i = 0; i < GRAPH_WIDTH; i++) {
    tempHistory[i] = initialTemp;
  }
  
  last_time = millis();
  windowStartTime = millis();
  lastTurnTime = millis();
  lastDHTRead = millis();
}

void loop() {
  unsigned long now = millis();

  // 1. Считывание DHT (раз в 1000 мс для стабильности датчика)
  if (now - lastDHTRead >= 1000) {
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    
    if (!isnan(h) && !isnan(t)) {
      currentHum = h;
      currentTemp = t;
      
      // Сдвигаем массив истории и добавляем новое значение
      for (int i = 0; i < GRAPH_WIDTH - 1; i++) {
        tempHistory[i] = tempHistory[i + 1];
      }
      tempHistory[GRAPH_WIDTH - 1] = (int16_t)(currentTemp * 10);
    }
    lastDHTRead = now;
  }

  // 2. ПД-регулятор (обновляем раз в 1 секунду)
  if (now - last_time >= 1000) {
    float dt = (now - last_time) / 1000.0;
    float error = setpoint - currentTemp;
    float derivative = (error - last_error) / dt;
    
    float outputPercent = basePower + (Kp * error) + (Kd * derivative);
    outputPercent = constrain(outputPercent, 0.0, 100.0);
    
    pdOutputMs = (outputPercent / 100.0) * windowSize;
    
    last_error = error;
    last_time = now;
    
    // Отладка в Serial
    Serial.print("T: "); Serial.print(currentTemp, 1);
    Serial.print(" | H: "); Serial.print(currentHum, 1);
    Serial.print(" | Err: "); Serial.print(error, 2);
    Serial.print(" | Pwr: "); Serial.print(outputPercent, 1);
    Serial.print(" | Turn: "); Serial.println(isTurning ? "YES" : "NO");
  }

  // 3. Управление реле (широтно-импульсное управление по времени)
  if (now - windowStartTime > windowSize) {
    windowStartTime += windowSize;
  }
  if (now - windowStartTime < pdOutputMs) {
    digitalWrite(RELAY_PIN, HIGH);
  } else {
    digitalWrite(RELAY_PIN, LOW);
  }

  // 4. Логика переворота яиц (НЕБЛОКИРУЮЩАЯ)
  // Запуск переворота, если пришло время и мы еще не крутим
  if (!isTurning && (now - lastTurnTime >= TURN_INTERVAL_MS)) {
    isTurning = true;
    stepsRemaining = STEPS_PER_TURN;
    lastStepTime = now;
    Serial.println(">>> Starting egg turn...");
  }

  // Выполнение шагов мотора
  if (isTurning && stepsRemaining > 0) {
    if (now - lastStepTime >= STEP_DELAY_MS) {
      // Подаем сигналы на пины согласно таблице
      for (int i = 0; i < 4; i++) {
        digitalWrite(stepPins[i], stepSequence[currentStep][i]);
      }
      
      currentStep = (currentStep + 1) % 8; // Переход к следующему шагу (0..7)
      stepsRemaining--;
      lastStepTime = now;
    }
  } 
  // Завершение переворота
  else if (isTurning && stepsRemaining == 0) {
    // Выключаем все катушки для экономии энергии и снижения нагрева драйвера ULN2003
    for (int i = 0; i < 4; i++) {
      digitalWrite(stepPins[i], LOW);
    }
    isTurning = false;
    lastTurnTime = now;
    Serial.println(">>> Egg turn complete.");
  }

    // ... (весь предыдущий код до отрисовки остается без изменений) ...

  // 5. Отрисовка интерфейса на OLED дисплее
  oled.clear();
  
  // Текстовая часть
  oled.setCursorXY(0, 0);
  oled.print("T:"); oled.print(currentTemp, 1); oled.print("C H:"); oled.print(currentHum, 0); oled.print("%");
  
  oled.setCursorXY(0, 8);
  float currentPowerPercent = (pdOutputMs / windowSize) * 100.0;
  oled.print("Trg:"); oled.print(setpoint, 1); oled.print("C P:"); oled.print(currentPowerPercent, 0); oled.print("%");

  // Статус переворота яиц
  oled.setCursorXY(0, 16);
  if (isTurning) {
    oled.print("TURNING... ");
    oled.print(STEPS_PER_TURN - stepsRemaining);
    oled.print("/");
    oled.print(STEPS_PER_TURN);
  } else {
    unsigned long minsToNext = (TURN_INTERVAL_MS - (now - lastTurnTime)) / 60000UL;
    oled.print("Next turn in ");
    oled.print(minsToNext);
    oled.print(" min");
  }
  
  // Графическая часть
  oled.line(0, 24, 127, 24);       
  oled.line(0, 63, 127, 63);       
  
  int targetY = getY((int16_t)(setpoint * 10));
  oled.line(0, targetY, 127, targetY);
  
  for (int i = 0; i < GRAPH_WIDTH - 1; i++) {
    int y1 = getY(tempHistory[i]);
    int y2 = getY(tempHistory[i + 1]);
    oled.line(i, y1, i + 1, y2);
  }
  
  // === ВАЖНОЕ ИЗМЕНЕНИЕ ===
  // Обновляем дисплей ТОЛЬКО если мотор не крутится.
  // Это предотвращает коллизию на шине I2C в момент электрических помех от мотора.
  if (!isTurning) {
    oled.update();
  }
  
  delay(50);
}
