#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);

#define BUTTON_PIN 18

unsigned long messageStartTime = 0;
const unsigned long messageDuration = 5000;
bool messageActive = false;

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;);
  }
  clearDisplay();
}

void loop() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    showMessage("message sent");
    messageStartTime = millis();
    messageActive = true;
    delay(300);
  }

  if (messageActive && (millis() - messageStartTime >= messageDuration)) {
    clearDisplay();
    messageActive = false;
  }
}

void showMessage(String msg) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 20);
  display.println(msg);
  display.display();
}

void clearDisplay() {
  display.clearDisplay();
  display.display();
}
