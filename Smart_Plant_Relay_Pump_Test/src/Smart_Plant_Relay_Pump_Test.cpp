/* 
 * Project myProject
 * Author: Your Name
 * Date: 
 */

#include "Particle.h"
#include "IoTTimer.h"
const int PUMPPIN = D3;
int currentTime;
int lastTime;
IoTTimer pumpTimer;

SYSTEM_MODE(SEMI_AUTOMATIC);

void setup() {
  
  pinMode(PUMPPIN, OUTPUT);
  lastTime = millis();

}

void loop() {

  currentTime = millis();
  if(currentTime - lastTime > 10000){
    digitalWrite(PUMPPIN, HIGH);
    pumpTimer.startTimer(500);
    lastTime = millis();
  }
  if(pumpTimer.isTimerReady()){
    digitalWrite(PUMPPIN, LOW);
  }
}
