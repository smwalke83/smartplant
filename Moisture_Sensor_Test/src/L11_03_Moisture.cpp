/* 
 * Project Moisture Sensor
 * Author: Scott Walker
 * Date: 2/9/2024
 */

#include "Particle.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
const int OLED_RESET = -1;
const int SENSORPIN = A1;
int moisture;
int currentTime, lastTime;
String DateTime, TimeOnly;
Adafruit_SSD1306 display(OLED_RESET);

SYSTEM_MODE(AUTOMATIC);

void setup() {
  
  Serial.begin();
  waitFor(Serial.isConnected, 10000);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.display();
  Time.zone(-7);
  Particle.syncTime();
  pinMode(SENSORPIN, INPUT);
  lastTime = -99999;

}

void loop() {

  currentTime = millis();
  if(currentTime - lastTime > 5000){
    moisture = analogRead(SENSORPIN);
    lastTime = millis();
    DateTime = Time.timeStr();
    TimeOnly = DateTime.substring(11, 19);
    Serial.printf("The moisture is %i.\n", moisture);
    Serial.printf("The Date/Time is %s.\n", DateTime.c_str());
    Serial.printf("The Time only is %s.\n", TimeOnly.c_str());
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.printf("The moisture value is %i.\nThe Date/Time is %s.\n", moisture, DateTime.c_str());
    display.display();
  }
  


}
