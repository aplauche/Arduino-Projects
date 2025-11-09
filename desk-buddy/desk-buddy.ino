#include <Arduino.h>
#include <FastLED.h>
#include <Wire.h>
#include "rgb_lcd.h"
#include "DFRobot_DF2301Q.h"
#include "DFRobotDFPlayerMini.h"
#define NUM_LEDS 48
#include <SoftwareSerial.h>
#include "sensirion_common.h"
#include "sgp30.h"

CRGB leds[NUM_LEDS];

rgb_lcd lcd;

// Voice recognition
// -------------------------
// WAKE: hello desk buddy (default "hello robot")
// DELETE: "I want to delete"
// DELETE EVERYTHING: "delete all"
// SET WAKE WORD: "learn wake word"
// SET COMMANDS: "learn command word"

//I2C communication for VR
DFRobot_DF2301Q_I2C asr;




// Assign MP3 Player Serial
#define FPSerial Serial1
//SoftwareSerial FPSerial(8, 9);  // RX=8, TX=9
DFRobotDFPlayerMini myDFPlayer;

// Color Array
CHSV colors[] = {
  CHSV(0, 255, 255),      // Red 0
  CHSV(32, 255, 255),      // Orange 1
  CHSV(64, 255, 255),     // Yellow 2
  CHSV(96, 255, 255),    // Green 3
  CHSV(128, 255, 255),     // Cyan 4
  CHSV(160, 255, 255),    // Blue 5
  CHSV(192, 255, 255),    // Purple 6
  CHSV(224, 255, 255)  // Pink 7
};


// Overall State of the System
bool isBreathActive = false;
bool isFocusTimerActive = false;
bool isNoiseActive = false;
bool isAlertActive = false;

// Button pins 
const int BTN_SELECT = 2;
const int BTN_SCROLL = 3;
const int BTN_BACK   = 4;

// Light Sensor Pin
const int pinLightSensor = A3;

// Debounce
unsigned long lastPress = 0;
const unsigned long debounceDelay = 500;

// CO2 Debounce
unsigned long lastAirQualityUpdate = 0;
const unsigned long airQualityUpdateDelay = 1000 * 2; // poll every 1 seconds for testing

// Light Level Debounce
unsigned long lastLightLevelUpdate = 0;
const unsigned long lightLevelUpdateDelay = 1000 * 5; // poll every 5 seconds for testing

// Lights
const int LEDDefaultBrightness = 100;
int LEDBrightness = 100;

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

// Debounce Voice Recognition
unsigned long lastVoiceUpdate = 0;
const unsigned long voiceUpdateDelay = 1000;


// Menu state
int currentMenu = 0;  // 0 = main, 1 = Lamp Color, 2 = Breath, 3 = Focus Timer
int currentItem = 0;

// Menu data
String mainMenu[] = {"Lamp Color", "Breath & Relax", "Focus Timer", "White Noise"}; // 4
String colorMenu[] = {"Red", "Orange", "Yellow", "Green", "Cyan", "Blue", "Purple", "Pink"}; // 8
String breathMenu[] = {"5 Breaths", "10 Breaths"}; // 2
String focusMenu[] = {"10 Minutes", "15 Minutes", "20 Minutes"}; // 3
String noiseMenu[] = {"On", "Off"}; // 2

// Helper for getting menu counts - better perf. hard-coded than dynamic
int getItemCount() {
  if (currentMenu == 0) return 4;     // main
  if (currentMenu == 1) return 8;     // color
  if (currentMenu == 2) return 2;     // breath
  if (currentMenu == 3) return 3;     // focus timer
  if (currentMenu == 4) return 2;     // noise
  return 0;
}

void setup() { 
  Wire.end();

  // MP3 player has to go first - for some reason it messes with I2C comms otherwise
  FPSerial.begin(9600);
  Serial.begin(115200);

  if (!myDFPlayer.begin(FPSerial, /*isACK = */true, /*doReset = */true)) {  //Use serial to communicate with mp3.
    Serial.println(F("Unable to begin:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));
    while(true);
  }
  Serial.println(F("DFPlayer Mini online."));
  
  myDFPlayer.setTimeOut(500); //Set serial communictaion time out 500ms

  myDFPlayer.volume(20);  //Set volume value. From 0 to 30

  delay(1000); // safety

  // Once these are done and won't conflict, start up LCD screen
  Wire.begin();
  // Wire.setClock(100000);

  // VOICE RECOGNITION CONFIG
  /**
    * @brief Set voice volume
    * @param voc - Volume value(1~7)
    */
  asr.setVolume(4);

  /**
      @brief Set mute mode
      @param mode - Mute mode; set value 1: mute, 0: unmute
  */
  asr.setMuteMode(0);

  /**
      @brief Set wake-up duration
      @param wakeTime - Wake-up duration (0-255)
  */
  asr.setWakeTime(20);

  delay(1000); // safety

  // Next set up the CO2 Sensor
  while (sgp_probe() != STATUS_OK) {
      Serial.println("SGP30 not found. Check wiring!");
      delay(500);
  }

  // Initialize air quality algorithm
  if (sgp_iaq_init() == STATUS_OK) {
      Serial.println("SGP30 initialized!");
  } else {
      Serial.println("SGP30 init failed!");
  }



  FastLED.addLeds<WS2812, 6, GRB>(leds, NUM_LEDS); 

  FastLED.setBrightness(LEDBrightness);
  for(int i=0; i<NUM_LEDS; i++){
    leds[i] = colors[5]; // Start blue
  }
  FastLED.show();

  lcd.begin(16, 2);
  defaultScreenColor();
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

  monitorAir();

  // Pause all activities by returning early if alert is active
  if(isAlertActive){
    return;
  }

  // Check for voice commands
  listen();

  // Run breath logic
  if(isBreathActive == true){
    boxBreath();
    return;
  }

  // Only check light levels if not currently doing breath exercises
  monitorLight();

  // Run task timer
  if(isFocusTimerActive == true){
    if (millis() - lastTimerUpdate < timerUpdateDelay) return;
    focusTimer();
  }

}

void reset(){
  currentMenu = 0;
  currentItem = 0;
  showMenu();
}

void defaultScreenColor(){
  lcd.setRGB(0, 0, 0);
}

void listen(){
  if (millis() - lastVoiceUpdate < voiceUpdateDelay) return;

  uint8_t CMDID = asr.getCMDID();

  if(CMDID == 0) return;

  switch (CMDID) {
    case 97: // Volume up                                               
      
      break;
    case 98: // Volume down                                               
      
      break;
    case 116: // Set to red                                              
      changeLEDColor(0);
      break;
    case 119: // Set to green                                              
      changeLEDColor(3);
      break;
    case 121: // Set to blue                                             
      changeLEDColor(5);
      break;
    case 5: // Do breathing Exercises                                             
      startBreathing(5);
      break;
    case 6: // Help me relax                                             
      startBreathing(5);
      break;
    case 7: // Set a focus timer                                               
      startFocusTimer(10);
      break;
    case 8: // Set a 5 minute focus timer                                               
      startFocusTimer(5);
      break;
    case 9: // Set a 10 minute focus timer                                               
      startFocusTimer(10);
      break;
    case 10: // Set a 20 minute focus timer                                              
      startFocusTimer(20);
      break;
    case 11: // Play white noise                                              
      playWhiteNoise();
      break;
    case 12: // Stop white noise                                               
      stopWhiteNoise();
      break;

    default:
      if (CMDID != 0) {
        Serial.print("CMDID = ");  //Printing command ID
        Serial.println(CMDID);
      }
  }

  lastVoiceUpdate = millis();
}

void monitorLight(){
  if (millis() - lastLightLevelUpdate < lightLevelUpdateDelay) return;

  int sensorValue = analogRead(pinLightSensor); 
  const int threshold = 200;
  const int buffer = 20; // prevent flickers

  // Debug brighness values
  Serial.println(sensorValue);

  if(sensorValue > threshold + buffer){
    Serial.println("Light level adequate!");
    if(LEDBrightness != LEDDefaultBrightness){
      LEDBrightness = LEDDefaultBrightness;
      FastLED.setBrightness(LEDBrightness);
      FastLED.show();
    }
  }
  if(sensorValue < threshold - buffer){
    Serial.println("Light level low!");
    if(LEDBrightness != 255){
      LEDBrightness = 255;
      FastLED.setBrightness(LEDBrightness);
      FastLED.show();
    }
  }
  lastLightLevelUpdate = millis();
}

void monitorAir(){
  if (millis() - lastAirQualityUpdate < airQualityUpdateDelay) return;
  s16 err;
  u16 tvoc_ppb, co2_eq_ppm;
  err = sgp_measure_iaq_blocking_read(&tvoc_ppb, &co2_eq_ppm);
  if (err == STATUS_OK) {
    if(co2_eq_ppm > 1000){
      isAlertActive = true;
      lcd.setRGB(255, 0, 0); // screen goes red
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("CO2 Level High");
      lcd.setCursor(0, 1);
      lcd.print(co2_eq_ppm);
      lcd.print(" ");
      lcd.print("ppm");
    }
  }
  lastAirQualityUpdate = millis();
}

void checkForButtonPresses(){
  if (millis() - lastPress < debounceDelay) return;

  // Simply cancel out alert if there is one
  if(isAlertActive){
    if(digitalRead(BTN_SELECT) == HIGH || digitalRead(BTN_SCROLL) == HIGH || digitalRead(BTN_BACK) == HIGH){
      lastPress = millis();
      isAlertActive = false;
      defaultScreenColor();
      if(!isBreathActive && !isFocusTimerActive){
        showMenu();
      }
    }
    return;
  }

  // If activity is running - only listen to back button
  if(isFocusTimerActive || isBreathActive){
    if(digitalRead(BTN_BACK) == HIGH){
      handleBack();
    }
    return;
  }

  // Otherwise normal menu operation
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

  // Defensive clear out, in case there was an alert
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Time To Lock In!");

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
    FastLED.setBrightness(LEDDefaultBrightness);
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
    int brightness = map(millis() - breathTimer, 0, breathDelay, 20, 220);
    FastLED.setBrightness(brightness);
    FastLED.show();
  } 
  
  if(breathState == 3) {
    int brightness = map(breathDelay - (millis() - breathTimer), 0, breathDelay, 20, 220);
    FastLED.setBrightness(brightness);
    FastLED.show();
  }
}

void playWhiteNoise(){
  myDFPlayer.play(1);  //Play the first mp3
  myDFPlayer.loop(1);  //Loop the first mp3
  lcd.clear();
  lcd.print("Noise On!");
}

void stopWhiteNoise(){
  myDFPlayer.pause();
  lcd.clear();
  lcd.print("Noise Off!");
}

void changeLEDColor(int colorIndex){
  for(int i=0; i<NUM_LEDS; i++){
    leds[i] = colors[colorIndex]; 
  }
  FastLED.show();
}

void startBreathing(int numBreaths){
  breathCount = numBreaths;
  isBreathActive = true;
}

void startFocusTimer(int minutes){
  focusTimerLength = 1000 * 60 * minutes;
  focusTimerStart = millis();
  isFocusTimerActive = true;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Time To Lock In!");
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
      changeLEDColor(currentItem);
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
        playWhiteNoise();
      } else {
        stopWhiteNoise();
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