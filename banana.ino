// for DF Pong, Wendy Meng

#include <DFPongController.h>
#include "Adafruit_APDS9999.h"

DFPongController controller;
Adafruit_APDS9999 apds;
int controllerNumber = 14;

// LED
int ledPin = 26;   //26 = A0


uint32_t lightValue = 0;

// light sensor values: dark
uint32_t darkValue = 150;
//uint32_t darkValue = 250;

// light sensor values: light
float brightValue = 800;
//float brightValue = 1700;

unsigned long calibrationTime = 3000;

// brightest reading counts 
float brightMargin = 0.9;

// reading speed
float adaptSpeed = 0.002;

//smoothed light reading
float smoothLight = 0;

//how smooth the paddle is (0.0 - 1.0)
float smoothing = 0.92;
float shownPos = 0.5;
float maxStep = 0.01;

// sensor
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

// banana color, size, sounds
  controller.setControllerName("BananaThief");   
  controller.setPlayerColor("#FFD23F");           
  controller.setHitSound("https://raw.githubusercontent.com/Wendy0416/banana-sounds/main/peelscore.mp3");
  controller.setScoreSound("https://raw.githubusercontent.com/Wendy0416/banana-sounds/main/chew2.mp3");  
  controller.setWinSound("https://raw.githubusercontent.com/Wendy0416/banana-sounds/main/bananawin.mp3");     
  controller.setMusic("https://raw.githubusercontent.com/Wendy0416/banana-sounds/main/bgm2.mp3");  
  controller.begin();
 controller.setPaddleSize(150);   
  //paddle size, 50 - 200 (bigger = easier to hit)
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

  controller.sendPosition((int)(pos * 1000), 1000, 0);

  delay(20);
}
