#include <FastLED.h>
#define NUM_LEDS 20
CRGB leds[NUM_LEDS];

CHSV hsv0 = CHSV(0, 255, 255);      // Red
CHSV hsv1 = CHSV(32, 255, 255);      // Orange
CHSV hsv2 = CHSV(64, 255, 255);     // Yellow
CHSV hsv3 = CHSV(96, 255, 255);    // Green
CHSV hsv4 = CHSV(128, 255, 255);     // Cyan
CHSV hsv5 = CHSV(160, 255, 255);    // Blue
CHSV hsv6 = CHSV(192, 255, 255);    // Purple
CHSV hsv7 = CHSV(224, 255, 255);    // Pink

void setup() { 
  FastLED.addLeds<WS2812, 6, GRB>(leds, NUM_LEDS); 

}




void loop() {
  for(int i=0; i<5; i++){
    leds[i] = hsv2; FastLED.show(); 
  }
  
  delay(2000);

  // leds[0] = CRGB::Blue; FastLED.show(); delay(500);
  // leds[10] = CRGB::Red; FastLED.show(); delay(500);
  // leds[10] = CRGB::Blue; FastLED.show(); delay(500);
}