#include <Wire.h>
#include "rgb_lcd.h"

rgb_lcd lcd;

// Button pins (Grove buttons are active HIGH)
const int BTN_SELECT = 2;
const int BTN_SCROLL = 3;
const int BTN_BACK   = 4;

// Debounce
unsigned long lastPress = 0;
const unsigned long debounceDelay = 200;

// Menu state
int currentMenu = 0;  // 0 = main, 1 = Noise, 2 = Breath, 3 = Focus Timer
int currentItem = 0;

// Menu data
String mainMenu[] = {"Noise", "Breath", "Focus Timer"};
String noiseMenu[] = {"Play", "Stop"};
String breathMenu[] = {"Box Breathe", "Another"};
String focusMenu[] = {"10 Min", "15 Min", "20 Min"};

void setup() {
  lcd.begin(16, 2);
  lcd.setRGB(0, 128, 255);
  lcd.clear();

  pinMode(BTN_SELECT, INPUT);
  pinMode(BTN_SCROLL, INPUT);
  pinMode(BTN_BACK, INPUT);

  lcd.print("Menu Ready");
  delay(1000);
  showMenu();
}

void loop() {
  if (millis() - lastPress < debounceDelay) return;

  if (digitalRead(BTN_SELECT) == HIGH) {
    handleSelect();
  } else if (digitalRead(BTN_SCROLL) == HIGH) {
    handleScroll();
  } else if (digitalRead(BTN_BACK) == HIGH) {
    handleBack();
  }
}

void handleSelect() {
  lastPress = millis();

  if (currentMenu == 0) { // Main menu
    currentMenu = currentItem + 1;
    currentItem = 0;
  } else {
    lcd.clear();
    lcd.print("Running...");
    delay(1000); // simulate action
  }

  showMenu();
}

void handleScroll() {
  lastPress = millis();

  int itemCount = getItemCount();
  currentItem = (currentItem + 1) % itemCount;

  showMenu();
}

void handleBack() {
  lastPress = millis();

  if (currentMenu != 0) {
    currentMenu = 0;
    currentItem = 0;
    showMenu();
  }
}

int getItemCount() {
  if (currentMenu == 0) return 3;     // main
  if (currentMenu == 1) return 2;     // noise
  if (currentMenu == 2) return 2;     // breath
  if (currentMenu == 3) return 3;     // focus timer
  return 0;
}

void showMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);

  if (currentMenu == 0) {
    lcd.print("Main Menu");
    lcd.setCursor(0, 1);
    lcd.print("> ");
    lcd.print(mainMenu[currentItem]);
  } else if (currentMenu == 1) {
    lcd.print("Noise");
    lcd.setCursor(0, 1);
    lcd.print("> ");
    lcd.print(noiseMenu[currentItem]);
  } else if (currentMenu == 2) {
    lcd.print("Breath");
    lcd.setCursor(0, 1);
    lcd.print("> ");
    lcd.print(breathMenu[currentItem]);
  } else if (currentMenu == 3) {
    lcd.print("Focus Timer");
    lcd.setCursor(0, 1);
    lcd.print("> ");
    lcd.print(focusMenu[currentItem]);
  }
}