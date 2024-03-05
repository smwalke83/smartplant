/* 
 * Project Dust and Air Quality Sensors
 * Author: Scott Walker
 * Date: 2/16/2024
 */

#include "Particle.h"
#include "math.h"
#include "Air_Quality_Sensor.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT/Adafruit_MQTT_SPARK.h"
#include "Adafruit_MQTT/Adafruit_MQTT.h"
#include "credentials.h"
TCPClient TheClient;
Adafruit_MQTT_SPARK mqtt(&TheClient,AIO_SERVER,AIO_SERVERPORT,AIO_USERNAME,AIO_KEY); 
Adafruit_MQTT_Publish aqPub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/air-quality");
Adafruit_MQTT_Publish dustPub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/dust");
const int AQPIN = A0;
//AirQualitySensor aqSensor(AQPIN);                         //I investigated the .cpp file and recreated it within this code.
//int current_quality =-1;
int rawValue;
const int DUSTPIN = D4;
const int SAMPLETIME = 30000;
int lastTime1;
int lastTime2;
unsigned int lowPulseOccupancy;
unsigned int duration;
float ratio;
float concentration;
int aqQuantitative(int x, int y, int *z, int *w);
int standardVoltage(int x, int y);
int currentVoltage;
int lastVoltage;
int voltageSum;
int volSumCount;
float standVoltage;
int lastStdVolUpdated;
int initVoltage;
int airValue;
void MQTT_connect();
bool MQTT_ping();

SYSTEM_MODE(SEMI_AUTOMATIC);

void setup() {
  
  Serial.begin(9600);
  waitFor(Serial.isConnected, 10000);
  WiFi.on();
  WiFi.connect();
  while(WiFi.connecting()) {
    Serial.printf(".");
  }
  Serial.printf("\n\n");
  pinMode(DUSTPIN, INPUT);
  pinMode(AQPIN, INPUT);
  //aqSensor.init();
  lastTime1 = millis();
  lastTime2 = millis();
  lowPulseOccupancy = 0;
  volSumCount = 0;
  voltageSum = 0;
  initVoltage = analogRead(AQPIN);
  currentVoltage = initVoltage;
  lastVoltage = currentVoltage;
  standVoltage = initVoltage;
  lastStdVolUpdated = millis();

}

void loop() {

  MQTT_connect();
  MQTT_ping();
  duration = pulseIn(DUSTPIN, LOW);
  lowPulseOccupancy = lowPulseOccupancy + duration;
  //current_quality = aqSensor.slope();
  lastVoltage = currentVoltage;
  currentVoltage = analogRead(AQPIN);
  rawValue = aqQuantitative(currentVoltage, lastVoltage, &voltageSum, &volSumCount);
  if(millis() - lastTime1 > SAMPLETIME){
    ratio = lowPulseOccupancy/(SAMPLETIME*10.0);
    concentration = 1.1*pow(ratio, 3) - 3.8*pow(ratio, 2) + 520*ratio + 0.62;
    Serial.printf("Ratio:%0.2f, Concentration:%0.2f\n", ratio, concentration);
    lastTime1 = millis();
    lowPulseOccupancy = 0;
    aqPub.publish(airValue);
    Serial.printf("Publishing %i \n", airValue);
    dustPub.publish(concentration);
    Serial.printf("Publishing %0.2f\n", concentration); 
  }
  if(millis() - lastTime2 > 5000){
    /*if (current_quality >= 0){
      if (current_quality==0){
        Serial.printf("High pollution! Force signal active.\n");
      }
      else if (current_quality==1){
        Serial.printf("High pollution!\n");
      }
      else if (current_quality==2){
        Serial.printf("Low pollution! \n");
      }
      else if (current_quality ==3){
        Serial.printf("Fresh air\n");
      }
    }*/
    if(millis() - lastStdVolUpdated > 500000){
      standardVoltage(voltageSum, volSumCount);
    }
    
    if (rawValue > 400 || currentVoltage > 700) {
      airValue = 0;
      Serial.printf("High Pollution - Force Signal! - %i\n", airValue);
    }
    else if ((rawValue > 400 && currentVoltage < 700) || currentVoltage - standVoltage > 150) {
      airValue = 1;
      Serial.printf("High Pollution - %i\n", airValue);
    }
    else if ((rawValue > 200 && currentVoltage < 700) || currentVoltage - standVoltage > 50) {
      airValue = 2;
      Serial.printf("Low Pollution - %i\n", airValue);
    }
    else {
      airValue = 3;
      Serial.printf("Fresh Air - %i\n", airValue);
    }
    lastTime2 = millis();
  }
  
}
int aqQuantitative(int x, int y, int *z, int *w){
  int result;
  result = x - y;
  *z += x;
  *w += 1;
  return result;
}
int standardVoltage(int x, int y){
  int result;
  result = x/y;
  x = 0;
  y = 0;
  return result;
}
bool MQTT_ping() {
  static unsigned int last;
  bool pingStatus;

  if ((millis()-last)>120000) {
      Serial.printf("Pinging MQTT \n");
      pingStatus = mqtt.ping();
      if(!pingStatus) {
        Serial.printf("Disconnecting \n");
        mqtt.disconnect();
      }
      last = millis();
  }
  return pingStatus;
}
void MQTT_connect() {
  int8_t ret;
 
  // Return if already connected.
  if (mqtt.connected()) {
    return;
  }
 
  Serial.print("Connecting to MQTT... ");
 
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.printf("Error Code %s\n",mqtt.connectErrorString(ret));
       Serial.printf("Retrying MQTT connection in 5 seconds...\n");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds and try again
  }
  Serial.printf("MQTT Connected!\n");
}
