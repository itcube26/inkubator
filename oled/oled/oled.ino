#include <GyverOLED.h>
GyverOLED<SSD1306_128x64, OLED_NO_BUFFER> oled;

void setup() {
  oled.init();        // инициализация
  oled.clear();       // очистка
  oled.setScale(3);   // масштаб текста (1..4)
  oled.home();        // курсор в 0,0
  oled.print("Привет!");

  delay(1000);

  oled.setScale(1);
  // курсор на начало 3 строки
  oled.setCursor(0, 3);
  oled.print("ЭТО умный инкубатор");

  // курсор на (20, 50)
  oled.setCursorXY(20, 50);
  float pi = 3.14;
  oled.print("PI = ");
  oled.print(pi);
}
void loop() {
}