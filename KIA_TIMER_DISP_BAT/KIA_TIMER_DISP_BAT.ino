#include <Wire.h>
#include "SSD1306Wire.h"

// Структура для песчинки
struct SandParticle {
  float x;      // координата по X — направление пересыпания (справа налево)
  float speed;  // пиксели/мс
  unsigned long born;
  bool active;
};

const int MAX_PARTICLES = 5;
SandParticle particles[MAX_PARTICLES];
unsigned long lastParticleTime = 0;
const unsigned long particleInterval = 300;  // новая песчинка каждые 300 мс

SSD1306Wire display(0x3c, 5, 4);  // Адрес, SDA, SCL

const int BUTTON_PIN = 0;

unsigned long timerEndMillis = 0;
bool timerActive = false;
bool timerExpired = false;

unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;
int lastButtonState = HIGH;
int currentButtonState;

unsigned long lastSandUpdate = 0;
const unsigned long sandUpdateInterval = 200;
int sandLevel = 0;  // 0 = весь песок справа, 100 = весь слева

void setup() {
  display.init();
  display.setContrast(255);
  display.setFont(ArialMT_Plain_10);

  // Убираем все flip'ы — рисуем в нативной ориентации 128x64
  // display.flipScreenVertically();
  // display.flipScreenHorizontally();

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  showStartScreen();

  for (int i = 0; i < MAX_PARTICLES; i++) {
    particles[i].active = false;
  }
}

void showStartScreen() {
  display.clear();
  display.drawString(2, 10, "Press button 0");
  display.drawString(8, 30, "to add 5 min");
  display.display();
  display.normalDisplay();
}

void loop() {
  checkButton();
  updateTimer();
  updateDisplay();
}

void checkButton() {
  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != currentButtonState) {
      currentButtonState = reading;

      if (currentButtonState == LOW) {
        if (timerExpired) {
          timerActive = false;
          timerExpired = false;
          sandLevel = 0;
          showStartScreen();
        } else {
          addTime(5);
        }
      }
    }
  }

  lastButtonState = reading;
}

void addTime(int minutes) {
  unsigned long addMillis = minutes * 60UL * 1000UL;

  if (!timerActive) {
    timerEndMillis = millis() + addMillis;
    timerActive = true;
    sandLevel = 0;
  } else {
    timerEndMillis += addMillis;
  }

  timerExpired = false;
}

void updateTimer() {
  if (timerActive && !timerExpired && millis() >= timerEndMillis) {
    timerExpired = true;
  }
}

void drawHourglass(int level) {
  display.clear();

  int y_center = 32;
  int left_x = 20;
  int right_x = 108;
  int base_height = 40;
  int neck_height = 6;

  // === Контур часов ===
  display.drawLine(right_x, y_center - base_height / 2, right_x, y_center + base_height / 2);
  display.drawLine(right_x, y_center - base_height / 2, 80, y_center - neck_height / 2);
  display.drawLine(right_x, y_center + base_height / 2, 80, y_center + neck_height / 2);

  display.drawLine(left_x, y_center - base_height / 2, left_x, y_center + base_height / 2);
  display.drawLine(left_x, y_center - base_height / 2, 40, y_center - neck_height / 2);
  display.drawLine(left_x, y_center + base_height / 2, 40, y_center + neck_height / 2);

  display.drawLine(40, y_center - neck_height / 2, 80, y_center - neck_height / 2);
  display.drawLine(40, y_center + neck_height / 2, 80, y_center + neck_height / 2);

  // === Песок справа — уходит от края к горлышку ===
  int sand_zone_right = right_x - 80;
  int sand_width_right = map(level, 0, 100, sand_zone_right, 0);
  if (sand_width_right > 0) {
    for (int x = 80; x < 80 + sand_width_right; x++) {
      int dist_from_neck = x - 80;
      int half_height = map(dist_from_neck, 0, sand_zone_right, neck_height / 2, base_height / 2);
      if (half_height > 0) {
        display.drawLine(x, y_center - half_height, x, y_center + half_height);
      }
    }
  }

  // === Песок слева — ОСТРЫЙ ТРЕУГОЛЬНИК, появляется БЫСТРО на начальном этапе ===
  int sand_zone_left = 40 - left_x;  // 20px

  // Нелинейное ускорение: квадратный корень для быстрого старта
  float level_f = (float)level;
  float accelerated_level = pow(level_f / 100.0f, 0.5f) * 100.0f;
  int sand_width_left = map((int)accelerated_level, 0, 100, 0, sand_zone_left);

  // Гарантируем минимальную видимую кучку
  if (level > 0 && sand_width_left < 3) {
    sand_width_left = 3;
  }

  if (sand_width_left > 0) {
    for (int x = left_x; x < left_x + sand_width_left; x++) {
      int dist_from_left = x - left_x;
      int half_height = map(dist_from_left, 0, sand_width_left, base_height / 2, 0);

      if (half_height > 0) {
        display.drawLine(x, y_center - half_height, x, y_center + half_height);
      }
    }
  }

  // === Анимация песчинок ===
  if (timerActive && !timerExpired) {
    unsigned long now = millis();

    if (now - lastParticleTime > particleInterval) {
      lastParticleTime = now;
      for (int i = 0; i < MAX_PARTICLES; i++) {
        if (!particles[i].active) {
          particles[i].active = true;
          particles[i].x = 88;
          particles[i].born = now;
          particles[i].speed = 48.0f / 1000.0f;
          break;
        }
      }
    }

    for (int i = 0; i < MAX_PARTICLES; i++) {
      if (particles[i].active) {
        float distance = particles[i].speed * (now - particles[i].born);
        particles[i].x = 88 - distance;

        int jitter = random(-2, 3);

        if (particles[i].x < left_x) {
          particles[i].active = false;
        } else {
          int x_int = (int)particles[i].x;
          display.setPixel(x_int, y_center + jitter);
        }
      }
    }
  }
}

void updateDisplay() {
  static unsigned long lastBlinkTime = 0;
  static bool invertedState = false;

  if (timerExpired) {
    if (millis() - lastBlinkTime > 250) {
      lastBlinkTime = millis();
      invertedState = !invertedState;

      display.clear();
      display.drawString(30, 20, "TIME'S UP!");
      display.drawString(20, 40, "Press button");
      display.display();

      if (invertedState) {
        display.invertDisplay();
      } else {
        display.normalDisplay();
      }
    }
  } else if (timerActive) {
    unsigned long remainingMillis = timerEndMillis - millis();
    unsigned long elapsedMillis = 0;

    if (millis() > (timerEndMillis - 5 * 60 * 1000UL)) {
      elapsedMillis = millis() - (timerEndMillis - 5 * 60 * 1000UL);
    }

    sandLevel = map(elapsedMillis, 0, 5 * 60 * 1000UL, 0, 100);
    if (sandLevel > 100) sandLevel = 100;

    if (millis() - lastSandUpdate > sandUpdateInterval) {
      lastSandUpdate = millis();
    }

    drawHourglass(sandLevel);  // Только песочные часы, без текста времени рядом!

    display.normalDisplay();
    display.display();
  }
}