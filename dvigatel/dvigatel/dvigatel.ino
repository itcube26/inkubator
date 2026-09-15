// Указание подключенных цифровых входов (остаются неизменными)
const int in1 = 4;
const int in2 = 5;
const int in3 = 6;
const int in4 = 7;

// Задержка между шагами (чем больше, тем медленнее вращение)
const int dl = 1;

// Массив шагов для вращения по часовой стрелке (на основе вашего кода)
const byte steps[7][4] = {
  {LOW,  LOW,  HIGH, HIGH}, // Шаг 0
  {LOW,  LOW,  HIGH, LOW }, // Шаг 1
  {LOW,  HIGH, HIGH, LOW }, // Шаг 2
  {HIGH, HIGH, LOW,  LOW }, // Шаг 3
  {HIGH, LOW,  LOW,  LOW }, // Шаг 4
  {HIGH, LOW,  LOW,  HIGH}, // Шаг 5
  {LOW,  LOW,  LOW,  HIGH}  // Шаг 6
};

unsigned long lastChangeTime = 0;
const unsigned long changeInterval = 5000; // Интервал смены направления (5 секунд)
bool isClockwise = true;                   // Текущее направление (true = по часовой)
int stepIndex = 0;                         // Текущий индекс шага

void setup() {
  pinMode(in1, OUTPUT);
  pinMode(in2, OUTPUT);
  pinMode(in3, OUTPUT);
  pinMode(in4, OUTPUT);
}

void loop() {
  unsigned long currentTime = millis();

  // Проверяем, прошло ли 5 секунд с последней смены направления
  if (currentTime - lastChangeTime >= changeInterval) {
    lastChangeTime = currentTime;
    isClockwise = !isClockwise; // Инвертируем направление
  }

  // Выбираем порядок шага: прямой для ЧС, обратный (6 - index) для против ЧС
  int currentStep = isClockwise ? stepIndex : (6 - stepIndex);

  // Подаем напряжение согласно выбранному шагу
  digitalWrite(in1, steps[currentStep][0]);
  digitalWrite(in2, steps[currentStep][1]);
  digitalWrite(in3, steps[currentStep][2]);
  digitalWrite(in4, steps[currentStep][3]);

  // Переходим к следующему шагу, зацикливая массив
  stepIndex++;
  if (stepIndex >= 7) {
    stepIndex = 0;
  }

  delay(dl);
}