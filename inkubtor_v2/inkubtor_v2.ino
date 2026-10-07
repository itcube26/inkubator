#include <DHT.h>
#include <GyverOLED.h>

// --- Настройки пинов ---
#define DHT_PIN 2
#define RELAY_PIN 12

// --- Настройки ПД-регулятора ---
float setpoint = 37.5;   // Целевая температура (°C)
float basePower = 20.0;  // Базовая мощность нагрева (%) для компенсации теплопотерь
float Kp = 10.0;         // Пропорциональный коэффициент (сила реакции на ошибку)
float Kd = 50.0;         // Дифференциальный коэффициент (гашение колебаний)

// Окно ШИМ-управления реле в мс. 
// Для механического реле: 5000–10000 мс (чтобы не щелкало слишком часто).
// Для SSR (твердотельного реле) или MOSFET: 100–500 мс.
unsigned long windowSize = 5000; 

// --- Инициализация компонентов ---
// Используем OLED_BUFFER вместо OLED_NO_BUFFER для быстрой и плавной отрисовки графика!
GyverOLED<SSD1306_128x64, OLED_BUFFER> oled;
DHT dht(DHT_PIN, DHT11);

// --- Переменные для графика ---
const int GRAPH_WIDTH = 128;
int16_t tempHistory[GRAPH_WIDTH]; // Храним температуру * 10 (экономия RAM)

// --- Переменные для ПД-регулятора ---
float last_error = 0;
unsigned long last_time = 0;
unsigned long windowStartTime = 0;
float pdOutputMs = 0; // Время включения реле в текущем окне (мс)

// Функция перевода температуры (x10) в координату Y на экране
// Диапазон температур графика: 30.0 ... 45.0 °C
// Диапазон Y на экране: 63 (низ) ... 24 (верх)
int getY(int16_t temp_x10) {
  int t = constrain(temp_x10, 300, 450);
  return 63 - ((t - 300) * 39) / 150; // Целочисленная математика для скорости и экономии памяти
}

void setup() {
  Serial.begin(9600);
  
  // Инициализация датчика и реле
  dht.begin();
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  
  // Приветственное сообщение на дисплее
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
}

void loop() {
  // 1. Считывание данных с датчика
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  
  if (isnan(h) || isnan(t)) {
    // При ошибке датчика используем последнее известное значение, чтобы не ломать регулятор
    t = tempHistory[GRAPH_WIDTH - 1] / 10.0;
  } else {
    // Сдвигаем массив истории и добавляем новое значение (умноженное на 10)
    for (int i = 0; i < GRAPH_WIDTH - 1; i++) {
      tempHistory[i] = tempHistory[i + 1];
    }
    tempHistory[GRAPH_WIDTH - 1] = (int16_t)(t * 10);
  }
  
  // 2. ПД-регулятор (обновляем раз в 1 секунду, т.к. тепловые процессы инерционны)
  unsigned long now = millis();
  if (now - last_time >= 1000) {
    float dt = (now - last_time) / 1000.0;
    float error = setpoint - t;
    float derivative = (error - last_error) / dt;
    
    // Расчет выхода: база + пропорция + дифференциал
    float outputPercent = basePower + (Kp * error) + (Kd * derivative);
    outputPercent = constrain(outputPercent, 0.0, 100.0); // Ограничиваем от 0 до 100%
    
    // Переводим проценты в миллисекунды включения внутри временного окна
    pdOutputMs = (outputPercent / 100.0) * windowSize;
    
    last_error = error;
    last_time = now;
    
    // Вывод в Serial для отладки и подстройки коэффициентов
    Serial.print("T: "); Serial.print(t, 1);
    Serial.print(" | H: "); Serial.print(h, 1);
    Serial.print(" | Err: "); Serial.print(error, 2);
    Serial.print(" | Pwr: "); Serial.println(outputPercent, 1);
  }
  
  // 3. Управление реле (широтно-импульсное управление по времени)
  if (now - windowStartTime > windowSize) {
    windowStartTime += windowSize; // Сдвигаем окно без дрейфа времени
  }
  
  if (now - windowStartTime < pdOutputMs) {
    digitalWrite(RELAY_PIN, HIGH); // Включаем нагрев
  } else {
    digitalWrite(RELAY_PIN, LOW);  // Выключаем нагрев
  }
  
  // 4. Отрисовка интерфейса на OLED дисплее
  oled.clear(); // Очищаем буфер перед новым кадром
  
  // Текстовая часть (верхние 24 пикселя)
  oled.setCursorXY(0, 0);
  oled.print("T:"); oled.print(t, 1); oled.print("C  H:"); oled.print(h, 0); oled.print("%");
  
  oled.setCursorXY(0, 8);
  float currentPowerPercent = (pdOutputMs / windowSize) * 100.0;
  oled.print("Trg:"); oled.print(setpoint, 1); oled.print("C  Pwr:"); oled.print(currentPowerPercent, 0); oled.print("%");
  
  // Графическая часть (Y от 24 до 63)
  oled.line(0, 24, 127, 24);       // Верхняя граница графика (45°C)
  oled.line(0, 63, 127, 63);       // Нижняя граница графика (30°C)
  
  // Линия целевой температуры (пунктиром или сплошной, здесь сплошная для простоты)
  int targetY = getY((int16_t)(setpoint * 10));
  oled.line(0, targetY, 127, targetY);
  
  // Отрисовка самого графика температуры
  for (int i = 0; i < GRAPH_WIDTH - 1; i++) {
    int y1 = getY(tempHistory[i]);
    int y2 = getY(tempHistory[i + 1]);
    oled.line(i, y1, i + 1, y2);
  }
  
  // Отправляем содержимое буфера на физический дисплей
  oled.update();
  
  // Небольшая задержка для стабильности работы датчика DHT
  delay(100);
}
