/*
DF Pong: Light with APDS-9999 (smoothed + auto calibration)
Based on the DFPongController example "dfPongStarterLight".
Changed to read brightness from the APDS-9999 instead of an LDR,
with smoothing, a log scale and auto calibration at start-up
(the APDS, smoothing, log scale and calibration parts, including adaptBright(), and the sound/look settings were written with Claude, Anthropic).

Bright = paddle up, dark = paddle down.

AUTO CALIBRATION:
For the first 3 seconds after the Feather turns on, leave the sensor
UNCOVERED if you can. The LED on A0 blinks fast while it measures.
After that it keeps adjusting itself while you play (see adaptBright()),
so you don't need to press reset when the light changes.

Open the Serial Monitor at 9600 baud to see the readings.
*/

#include <DFPongController.h>
#include "Adafruit_APDS9999.h"

DFPongController controller;
Adafruit_APDS9999 apds;

//your controller number (1 - 242): every player has a different one
int controllerNumber = 14;

//the pin your own LED is plugged into (long leg here, short leg to GND)
//blinks fast while calibrating, blinks while looking for the game, stays on when connected
int ledPin = 26;   //26 = A0

//light reading
uint32_t lightValue = 0;

//the reading when the sensor is fully covered: change this to yours
uint32_t darkValue = 150;
//uint32_t darkValue = 250;

//the reading in bright light: set at start-up, then keeps adjusting itself
float brightValue = 800;
//float brightValue = 1700;

//how long to calibrate, in milliseconds
unsigned long calibrationTime = 3000;

//how much of the brightest reading counts as "fully bright" (0.0 - 1.0)
//lower = easier to reach the top
float brightMargin = 0.9;

//how fast brightValue follows a dimmer room while you play (0.0 - 1.0)
//bigger = adapts faster, smaller = steadier
float adaptSpeed = 0.002;

//smoothed light reading
float smoothLight = 0;

//how smooth the paddle is (0.0 - 1.0)
//closer to 1.0 = smoother but slower, closer to 0 = faster but jumpier
float smoothing = 0.92;
float shownPos = 0.5;
float maxStep = 0.01;

//reads the brightness (green channel) from the APDS-9999
//returns the last good reading if a read fails
uint32_t readLight()
{
  uint32_t r, g, b, ir;
  if (apds.getRGBIRData(&r, &g, &b, &ir))
  {
    lightValue = g;
  }
  return lightValue;
}

//measures the light for a few seconds and sets brightValue from the brightest reading
void calibrateBright()
{
  Serial.println("Calibrating: leave the sensor uncovered...");
  pinMode(ledPin, OUTPUT);

  uint32_t brightest = 0;
  unsigned long start = millis();

  while (millis() - start < calibrationTime)
  {
    uint32_t reading = readLight();
    if (reading > brightest)
    {
      brightest = reading;
    }

    //blink the LED fast so you know it's calibrating
    digitalWrite(ledPin, (millis() / 100) % 2);
    delay(20);
  }
  digitalWrite(ledPin, LOW);

  //only use the new value if it is clearly brighter than the dark value
  if (brightest > darkValue * 3)
  {
    brightValue = brightest * brightMargin;
    Serial.print("Calibrated! brightValue = ");
    Serial.println(brightValue);
  }
  else
  {
    Serial.print("Too dark to calibrate, keeping brightValue = ");
    Serial.println(brightValue);
  }

  //start the smoothing from the bright value so the paddle doesn't jump
  smoothLight = brightValue;
}

//keeps adjusting brightValue while you play, so you never need to reset:
//- a brighter reading than before raises brightValue straight away
//- while the sensor is clearly uncovered, brightValue slowly drifts toward
//  the current light, so it settles down again in a dimmer room
//- while the sensor is covered (peel closed), nothing changes
void adaptBright()
{
  //brighter than the current "fully bright": move the bright end up
  if (lightValue * brightMargin > brightValue)
  {
    brightValue = lightValue * brightMargin;
  }
  //clearly uncovered: drift slowly toward the current light
  else if (lightValue > darkValue * 4)
  {
    float target = lightValue * brightMargin;
    brightValue = brightValue - (brightValue - target) * adaptSpeed;
  }

  //never let the bright end get too close to the dark end
  if (brightValue < darkValue * 3)
  {
    brightValue = darkValue * 3;
  }
}

void setup()
{
  Serial.begin(9600);
  delay(500);

  //start the APDS-9999 (plugged in with the STEMMA QT cable)
  if (!apds.begin())
  {
    Serial.println("No APDS-9999 found: check the STEMMA QT cable");
  }
  apds.enableLightSensor(true);   //turn on the light sensor
  apds.setRGBMode(true);          //read red, green, blue and IR
  delay(200);                     //give the sensor time for its first reading

  //measure the room's light before starting Bluetooth
 calibrateBright();

  controller.setControllerNumber(controllerNumber);
  controller.setStatusLED(ledPin);   //connection light on your own LED

  //how the banana looks and sounds in the game (played on the computer's speakers)
  //preview every sound here: https://digitalfuturesocadu.github.io/df-pong/game/cc26/builder/
  //the 3 sound files live in the public GitHub repository Wendy0416/banana-sounds
  //peel sound: Pixabay (free licence); supermarket sound: BigSoundBank (CC0); "Banana!" recorded by me
  controller.setControllerName("BananaThief");   //up to 20 characters
  controller.setPlayerColor("#FFD23F");               //banana yellow paddle
  controller.setHitSound("https://raw.githubusercontent.com/Wendy0416/banana-sounds/main/peelscore.mp3");
  controller.setScoreSound("https://raw.githubusercontent.com/Wendy0416/banana-sounds/main/chew2.mp3");   //when you score: banana peel
  controller.setWinSound("https://raw.githubusercontent.com/Wendy0416/banana-sounds/main/bananawin.mp3");     //when you win: "Banana!"
  controller.setMusic("https://raw.githubusercontent.com/Wendy0416/banana-sounds/main/bgm2.mp3");   //background: supermarket sounds
  controller.begin();
 controller.setPaddleSize(150);   //paddle size, 50 - 200 (bigger = easier to hit)
}

void loop()
{
  //keeps the connection to the game going: needed every loop
  controller.update();

  //read and smooth the light
  readLight();
  adaptBright();
  smoothLight = smoothLight * smoothing + lightValue * (1.0 - smoothing);

  Serial.print("Light: ");
  Serial.print(lightValue);
  Serial.print("   Smoothed: ");
  Serial.print(smoothLight);
  Serial.print("   Bright: ");
  Serial.println(brightValue);

  //keep the value inside its range
  float lightNow = constrain(smoothLight, darkValue, brightValue);

  //log scale: makes the middle of the range easier to hold
  float pos = (log(lightNow) - log(darkValue)) / (log(brightValue) - log(darkValue));

  //1000 = bright = top, 0 = dark = bottom
  controller.sendPosition((int)(pos * 1000), 1000, 0);

  delay(20);
}