#include <Arduino.h>
#include <FastLED.h>

#define LED_PIN     7     // Connect to the Data In (DI) of WS2812B
#define ANALOG_PIN  A1    // potentiometer pin
#define WIDTH       16    // Num LEDs wide
#define HEIGHT      16    // Num LEDs tall
#define NUM_LEDS    (WIDTH * HEIGHT) // panel's total LED count
#define BRIGHTNESS  20    // Adjust brightness (0-255)
#define LED_TYPE    WS2812B // Name of LED panel 
#define COLOR_ORDER GRB
#define avgCount    20    // # values to use to determine average potentiometer position (combat variability in output)
#define maxR_SbC    7     // Max radius away subcritical lights can go
#define pSbC        0.16  // Probability a subcritical light lights up
#define pC          0.4   // Probability a critical light lights up 
#define pSpC        1.00  // Probability a supercritical light lights up
#define fadeDist    2     // # radii behind current rad to start fade to black  

CRGB leds[NUM_LEDS], color;
int sensorValue;
int startX, startY, r, i, x, y;       // Initialize these variable to randomize each loop
bool full;
float probability;

void setup() {            // Need this to get everything ready 
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);
  randomSeed(analogRead(0));
}

// Turns all LEDs off
void clearBoard() {       
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
}

// Convert a snake-index to (x,y) coordinates.
void indexToXY(int index, int &x, int &y) {
  y = index / WIDTH;
  int pos = index % WIDTH;
  // Even rows go left-to-right, odd rows right-to-left.
  if (y % 2 == 0) {
    x = pos;
  } else {
    x = WIDTH - 1 - pos;
  }
}

// Convert (x,y) coordinates to snake-index.
int xyToIndex(int x, int y) {
  if (y % 2 == 0) {
    return y * WIDTH + x;
  } else {
    return y * WIDTH + (WIDTH - 1 - x);
  }
}

// Fades lights off a set distance behind the currently lit up ring 
void fadePreviousRingsSmoothly(int centerX, int centerY, int currentRadius, int fadeStart, int fadeToOff) {
  int fadeEnd = fadeStart + fadeToOff;

  for (int r = currentRadius - fadeEnd; r < currentRadius - fadeStart; r++) {
    if (r < 1) continue;
    float fadeProgress = float(currentRadius - r - fadeStart) / fadeToOff; // 0.0 to 1.0
    uint8_t fadeAmount = fadeProgress * 255;

    for (int x = centerX - r; x <= centerX + r; x++) {
      for (int y = centerY - r; y <= centerY + r; y++) {
        if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
          float dist = sqrt((x - centerX) * (x - centerX) + (y - centerY) * (y - centerY));
          if (fabs(dist - r) <= 0.5) {
            int index = xyToIndex(x, y);
            leds[index].fadeToBlackBy(fadeAmount);
          }
        }
      }
    }
  }

  // Immediately clear anything older than the fade zone
  for (int r = 1; r < currentRadius - fadeEnd; r++) {
    for (int x = centerX - r; x <= centerX + r; x++) {
      for (int y = centerY - r; y <= centerY + r; y++) {
        if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
          float dist = sqrt((x - centerX) * (x - centerX) + (y - centerY) * (y - centerY));
          if (fabs(dist - r) <= 0.5) {
            int index = xyToIndex(x, y);
            leds[index] = CRGB::Black;
          }
        }
      }
    }
  }
}

void lightCircularRippleWithSmoothTrail(
  int startIndex,
  CRGB color,
  int fadeStart = 2,
  int fadeToOff = 3,
  int delayMs = 100,
  float probability = 1.0,
  int maxRadius = -1  // -1 means no maximum radius (default)
) {
  int centerX, centerY;
  indexToXY(startIndex, centerX, centerY);

  float r = 1;
  const float ringThickness = 0.5;

  while (true) {
    if (maxRadius != -1 && r > maxRadius) break;

    bool litSomething = false;

    // Light the current ring with patchiness
    for (int x = 0; x < WIDTH; x++) {
      for (int y = 0; y < HEIGHT; y++) {
        float dx = x - centerX;
        float dy = y - centerY;
        float dist = sqrt(dx * dx + dy * dy);

        if (fabs(dist - r) <= ringThickness && random(1000) < probability * 1000) {
          int index = xyToIndex(x, y);
          // CRGB dimmedColor = color;
          // dimmedColor.nscale8_video(FastLED.getBrightness());  // Can maunally dim colors 
          leds[index] = color;
          litSomething = true;
        }
      }
    }

    // Smooth fade trailing rings
    fadePreviousRingsSmoothly(centerX, centerY, r, fadeStart, fadeToOff);

    FastLED.show();
    delay(delayMs);

    if (!litSomething) break;
    r++;
  }
}

// Function to fade all LEDs to black
void fadeAllToBlack() {
  for (int i = 0; i < 50; i++) { // Adjust the number of iterations for desired fade duration
    fadeToBlackBy(leds, NUM_LEDS, 20); // Fade by a small amount each iteration
    FastLED.show(); // Update the LEDs to reflect the fading
    delay(30); // Delay to control the speed of fading
  }
}

void loop() {
  // Safety catch to help eliminate noise in the sensor system and identify true orientation
  // --> Current state is based on the average of the last several sensor values
  sensorValue = 0;
  for (int i = 0; i < avgCount; i++) {
    sensorValue += analogRead(ANALOG_PIN);
  }
  sensorValue /= avgCount;

  i = random(NUM_LEDS);

  if (sensorValue < 341) { // Lower third is subcritical 
    lightCircularRippleWithSmoothTrail(i, CRGB::Blue, 1, 4, 100, pSbC, maxR_SbC);
    fadeAllToBlack();
    delay(100); // Subcritical should look S L O W 
  } 
  else if (sensorValue < 682) { // Middle third is critical
    lightCircularRippleWithSmoothTrail(i, CRGB::Chartreuse, 1, 7, 100, pC);
    fadeAllToBlack();
  } 
  else { // Upper third is super critical
    lightCircularRippleWithSmoothTrail(i, CRGB::DarkRed, 1, 13, 50, pSpC);
    //fadeAllToBlack();
  }

  clearBoard(); // Fresh start each round
} 

