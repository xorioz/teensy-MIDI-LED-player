/*
  I would like to mention that we are not using classes and any advanced oop.
  Doing so is somewhat resource intensive and would result in unacceptable timing delays in the main loop.
  Already using the two libraries have coaused a 10 ms delay on each frame.
  For now this is ok but if the delay increases to above 100 ms we will most likely have sytncing problems.

  This is just my assesment but i may be wrong.
  Normally you would not worry so much about this but considering that this is ment for syncing led effects
  and annimations to music via midi, it is very important to have a fast library so we don't get a que on the midi commands.
  lets put this on git
*/

#include "FastLED.h" // library for controlling the led strip.

// LED signal vars used to make note on and off messages visible on the teensy led
const int ledPin =  LED_BUILTIN;// the number of the LED pin
int ledState = LOW;             // ledState used to set the LED


#define NUM_LEDS 300 //number on leds on a single strip.
#define LED_PIN 1 // data pin
CRGB LEDs[NUM_LEDS]; // led array for fastled

//animation note names
#define SOLID 127
#define FLASH 126
#define PULSE 125

// color struct for saving colors recived over midi
struct LED {
  uint_least8_t G, R, B = 0;
} Color_1, Color_2, Color_3 ;

// struct that manages individual animation vars
struct ANIMATION {
  bool isRunning = false;
  uint_least8_t speedMultiplier = 1;
  long elapsedTime = 0;
} Animation[128];


//global timing vars
int ClockCount = 0; // counter used to mesure midi clock
long ClockTime = 1; // bpm mesured in milliseconds. can't be 0 to avoid devide by 0
long StartClockTime = 0;
long DeltaTime = 0;
long PreviusTime = 0;

//callbacks
void myStart() { ClockCount = 0; }
void myStop() { }
void myContinue() { ClockCount = 0; }


// this function is not entirely accurate at messuring the bpm
// my hope is that it averages out
void myClock() {
  if (ClockCount == 0) StartClockTime = millis();
  ClockCount++;

  if (ClockCount > 23) {
    ClockCount = 0;
    ClockTime = millis() - StartClockTime + DeltaTime;
    Serial.println(60000 / (ClockTime));
  }
}

// using controller 20 to 28 we get 3 colors for use in animations.
void myControlChange(byte channel, byte controller, byte value) {
  uint_least8_t cha = (uint_least8_t)channel;
  uint_least8_t val = (uint_least8_t)value;
  if (cha == 1) {
    switch (controller) {
      // set used color values
      case 20: Color_1.R = val << 1; break;
      case 21: Color_1.G = val << 1; break;
      case 22: Color_1.B = val << 1; break;
      case 23: Color_2.R = val << 1; break;
      case 24: Color_2.G = val << 1; break;
      case 25: Color_2.B = val << 1; break;
      case 26: Color_3.R = val << 1; break;
      case 27: Color_3.G = val << 1; break;
      case 28: Color_3.B = val << 1; break;
      default: break;
    }
  }

}

// on note on we set the running animation to true for the corosponding key
void myNoteOn(byte channel, byte note, byte velocity) {
  uint_least8_t cha = (uint_least8_t)channel;
  uint_least8_t no = (uint_least8_t)note;
  uint_least8_t vel = (uint_least8_t)velocity;
  uint_least8_t animationSpeed = 1;

  if (cha == 1) {
    ledState = HIGH; // teensy led

    // generate the speed mult for animation based on velocity
    if(vel > 0) {
    animationSpeed = (vel / 16) + ((vel % 16) ? 1 : 0);
    } else {
    animationSpeed = 1;
    }



    Animation[no].isRunning = true;
    Animation[no].speedMultiplier = animationSpeed;
  }
}

// on note of we stop the running animation and reset any relevent vars
void myNoteOff(byte channel, byte note, byte velocity) {
  uint_least8_t cha = (uint_least8_t)channel;
  uint_least8_t no = (uint_least8_t)note;

  if (cha == 1) {
    ledState = LOW; // teensy led
    Animation[no].isRunning = false;
    Animation[no].elapsedTime = 0;
  }
}

// reused animation functions
void mixRGB(int led, uint_least8_t R, uint_least8_t G, uint_least8_t B) { // for mixing annimations
  if (R > LEDs[led].r) LEDs[led].r = R;
  if (G > LEDs[led].g) LEDs[led].g = G;
  if (B > LEDs[led].b) LEDs[led].b = B;
}


// animations
// my hope is that once the framework is done this section should be the only one to edit.
// for now you also have to register the animation note name in all caps at the top
// and add an if statement in the main loop at the bottom.
/*=============================================================================*/
// flash animation. runs only once
void flash(long DT) {
  uint_least8_t R = 0, G = 0, B = 0;
  if (Animation[FLASH].elapsedTime == 0) {
    R = Color_1.R;
    G = Color_1.G;
    B = Color_1.B;
  }

  float flashTime = (ClockTime * Animation[FLASH].speedMultiplier) / 4; // devide by 4 so the first 4 of the speedMultiplier is halfing the animation length instead instead.
  if (Animation[FLASH].elapsedTime < flashTime) {
    float intensity = (flashTime - (float)Animation[FLASH].elapsedTime) / flashTime;
    R = (uint_least8_t)(Color_1.R * intensity);
    G = (uint_least8_t)(Color_1.G * intensity);
    B = (uint_least8_t)(Color_1.B * intensity);
  }
  for (int i = 0; i < NUM_LEDS; i++) {
    mixRGB(i, R, G, B);
  }
  Animation[FLASH].elapsedTime += DT;
}


/*=============================================================================*/
// setup
void setup() {
  Serial.begin(9600);
  pinMode(ledPin, OUTPUT);

  usbMIDI.setHandleClock(myClock);
  usbMIDI.setHandleStart(myStart);
  usbMIDI.setHandleContinue(myContinue);
  usbMIDI.setHandleStop(myStop);
  usbMIDI.setHandleNoteOff(myNoteOff);
  usbMIDI.setHandleNoteOn(myNoteOn);
  usbMIDI.setHandleControlChange(myControlChange);

  FastLED.addLeds<WS2812B, LED_PIN, GRB> (LEDs, NUM_LEDS);
  FastLED.setBrightness(255);
  //FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);
  FastLED.clear();
  FastLED.show();
}


// main loop

void loop() {
  PreviusTime = millis();
  while (usbMIDI.read()) {}

  // run animations
  if (Animation[FLASH].isRunning) flash(DeltaTime);

  digitalWrite(ledPin, ledState);
  FastLED.show();
  FastLED.clear();
  DeltaTime = millis() - PreviusTime;
}
