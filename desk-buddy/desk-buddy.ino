#include <FastLED.h>
#include <Wire.h>
#include "rgb_lcd.h"
#include "DFRobot_DF2301Q.h"
#include "DFRobotDFPlayerMini.h"
#define NUM_LEDS 48
#include <SoftwareSerial.h>

CRGB leds[NUM_LEDS];

rgb_lcd lcd;

// Assign MP3 Player Serial
#define FPSerial Serial1
//SoftwareSerial FPSerial(8, 9);  // RX=8, TX=9
DFRobotDFPlayerMini myDFPlayer;

// Color Array
CHSV colors[] = {
  CHSV(0, 255, 255),      // Red
  CHSV(32, 255, 255),      // Orange
  CHSV(64, 255, 255),     // Yellow
  CHSV(96, 255, 255),    // Green
  CHSV(128, 255, 255),     // Cyan
  CHSV(160, 255, 255),    // Blue
  CHSV(192, 255, 255),    // Purple
  CHSV(224, 255, 255)  // Pink
};


// Overall State of the System
bool isBreathActive = false;
bool isFocusTimerActive = false;
bool isNoiseActive = false;

// Button pins 
const int BTN_SELECT = 2;
const int BTN_SCROLL = 3;
const int BTN_BACK   = 4;

// Debounce
unsigned long lastPress = 0;
const unsigned long debounceDelay = 500;

// Lights
int LEDBrightness = 125;

// breathwork
int breathCount = 0; // how many breaths to run through
unsigned long breathTimer = 0;
unsigned long breathDelay = 3000;
int breathState = 0;
String breathMessages[] = {"Hold", "Inhale", "Hold", "Exhale"};

// Focus Timer
unsigned long focusTimerStart = 0;
unsigned long focusTimerLength = 0;
// Debounce
unsigned long lastTimerUpdate = 0;
const unsigned long timerUpdateDelay = 1000;

// Menu state
int currentMenu = 0;  // 0 = main, 1 = Lamp Color, 2 = Breath, 3 = Focus Timer
int currentItem = 0;

// Menu data
String mainMenu[] = {"Lamp Color", "Breath & Relax", "Focus Timer", "White Noise"}; // 0
String colorMenu[] = {"Red", "Orange", "Yellow", "Green", "Cyan", "Blue", "Purple", "Pink"}; // 1
String breathMenu[] = {"5 Breaths", "10 Breaths"}; // 2
String focusMenu[] = {"10 Minutes", "15 Minutes", "20 Minutes"}; // 3
String noiseMenu[] = {"On", "Off"}; // 2

void setup() { 
  Wire.end();
  FPSerial.begin(9600);

  if (!myDFPlayer.begin(FPSerial, /*isACK = */true, /*doReset = */true)) {  //Use serial to communicate with mp3.
    Serial.println(F("Unable to begin:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));
    while(true);
  }
  Serial.println(F("DFPlayer Mini online."));
  
  myDFPlayer.setTimeOut(500); //Set serial communictaion time out 500ms

  myDFPlayer.volume(20);  //Set volume value. From 0 to 30

  Wire.begin();


  FastLED.addLeds<WS2812, 6, GRB>(leds, NUM_LEDS); 

  FastLED.setBrightness(LEDBrightness);
  for(int i=0; i<NUM_LEDS; i++){
    leds[i] = colors[0]; 
  }
  FastLED.show();

  lcd.begin(16, 2);
  lcd.setRGB(255, 50, 0);
  lcd.clear();

  pinMode(BTN_SELECT, INPUT);
  pinMode(BTN_SCROLL, INPUT);
  pinMode(BTN_BACK, INPUT);

  lcd.print("Hi There :)");
  delay(2000);
  showMenu();



}




void loop() {

  checkForButtonPresses();

  // monitorAir();

  // monitorLight();

  // Run breath logic
  if(isBreathActive == true){
    boxBreath();
  }

  // Run task timer
  if(isFocusTimerActive == true){
    if (millis() - lastTimerUpdate < timerUpdateDelay) return;
    focusTimer();
  }

  // Run white noise
  // if(isNoiseActive == true){
  //   whiteNoise();
  // }

}

void reset(){
  currentMenu = 0;
  currentItem = 0;
  showMenu();
}

void checkForButtonPresses(){
  if (millis() - lastPress < debounceDelay) return;

  if (digitalRead(BTN_SELECT) == HIGH) {
    handleSelect();
  } else if (digitalRead(BTN_SCROLL) == HIGH) {
    handleScroll();
  } else if (digitalRead(BTN_BACK) == HIGH) {
    handleBack();
  }
}

void focusTimer(){

  if(millis() - focusTimerStart > focusTimerLength){
    // Done!
    isFocusTimerActive = false;
    reset();
    return;
  }
  unsigned long remaining = focusTimerLength - (millis() - focusTimerStart);

  unsigned int seconds = remaining / 1000;
  unsigned int minutes = seconds / 60;
  seconds = seconds % 60;

  // Print MM:SS
  lcd.setCursor(0, 1);
  
  if (minutes < 10) lcd.print('0');
  lcd.print(minutes);
  lcd.print(':');
  if (seconds < 10) lcd.print('0');
  lcd.print(seconds);
  lcd.print(" Remaining");
}

void boxBreath(){
  if(breathCount <= 0){
    isBreathActive = false;
    breathCount = 0;
    breathTimer = 0;
    breathState = 0;
    FastLED.setBrightness(LEDBrightness);
    FastLED.show();
    reset();
    return;
  }

  // Check delay and increment state
  if(millis() - breathTimer > breathDelay){
    if(breathState < 3){
      breathState = breathState + 1;
    } else {
      breathState = 0;
      breathCount = breathCount - 1;
    }
    // Display message
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Breathe Deeply");
    lcd.setCursor(0, 1);
    lcd.print(breathMessages[breathState]);

    // reset timer
    breathTimer = millis();
  }

  // Switch to handle the LED Fade
  if(breathState == 1) {
    int brightness = map(millis() - breathTimer, 0, breathDelay, 20, 255);
    FastLED.setBrightness(brightness);
    FastLED.show();
  } 
  
  if(breathState == 3) {
    int brightness = map(breathDelay - (millis() - breathTimer), 0, breathDelay, 20, 255);
    FastLED.setBrightness(brightness);
    FastLED.show();
  }
}


void handleSelect() {
  lastPress = millis();

  if (currentMenu == 0) { // Main menu - moving into submenus
    currentMenu = currentItem + 1;
    currentItem = 0;
  } else {

  // -----------------------
  // Running submenu tasks
  // -----------------------
    // Lamp Color
    if(currentMenu == 1){
      for(int i=0; i<NUM_LEDS; i++){
        leds[i] = colors[currentItem]; 
      }
      FastLED.show();
      return;
    }
    // Breath
    if(currentMenu == 2){
      if(currentItem == 0){
        breathCount = 5;
      }
      if(currentItem == 1){
        breathCount = 10;
      }
      isBreathActive = true;
      return;
    }

    // Focus
    if(currentMenu == 3){
      if(currentItem == 0){
        focusTimerLength = 1000 * 60  * 10;
      }
      if(currentItem == 1){
        focusTimerLength = 1000 * 60 * 15;
      }
      if(currentItem == 2){
        focusTimerLength = 1000 * 60  * 20;
      }
      focusTimerStart = millis();
      isFocusTimerActive = true;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Time To Lock In!");
      return;

    }

    if(currentMenu == 4){
      if(currentItem == 0){
        myDFPlayer.play(1);  //Play the first mp3
        myDFPlayer.loop(1);  //Loop the first mp3
        lcd.clear();
        lcd.print("Noise On!");

      } else {
        myDFPlayer.pause();
        lcd.clear();
        lcd.print("Noise Off!");
      }

      delay(1000); 
      reset();
      return;
    }



    // Simulate for others...
    lcd.clear();
    lcd.print("Running...");
    delay(1000); // simulate action
  }

  showMenu();
}


int getItemCount() {
  if (currentMenu == 0) return 4;     // main
  if (currentMenu == 1) return 8;     // color
  if (currentMenu == 2) return 2;     // breath
  if (currentMenu == 3) return 3;     // focus timer
  if (currentMenu == 4) return 2;     // noise
  return 0;
}

void handleScroll() {
  lastPress = millis();

  int itemCount = getItemCount();
  currentItem = (currentItem + 1) % itemCount;

  showMenu();
}

void handleBack() {
  lastPress = millis();

  // Make sure blocking activities are cancelled - white noise can continue
  isFocusTimerActive = false;
  isBreathActive = false;
  reset();

  // if (currentMenu != 0) {
  //   currentMenu = 0;
  //   currentItem = 0;
  //   showMenu();
  // }
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
    lcd.print("Lamp Color");
    lcd.setCursor(0, 1);
    lcd.print("> ");
    lcd.print(colorMenu[currentItem]);
  } else if (currentMenu == 2) {
    lcd.print("Breath & Relax");
    lcd.setCursor(0, 1);
    lcd.print("> ");
    lcd.print(breathMenu[currentItem]);
  } else if (currentMenu == 3) {
    lcd.print("Focus Timer");
    lcd.setCursor(0, 1);
    lcd.print("> ");
    lcd.print(focusMenu[currentItem]);
  } else if (currentMenu == 4) {
    lcd.print("White Noise");
    lcd.setCursor(0, 1);
    lcd.print("> ");
    lcd.print(noiseMenu[currentItem]);
  }
}