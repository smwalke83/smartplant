/* 
 * Project Smart Houseplant
 * Author: Scott Walker
 * Date: 2/23/24
 */

#include "Particle.h"
#include "IotClassroom_CNM.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#include "Adafruit_BME280.h"
#include "math.h"
#include "Air_Quality_Sensor.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT/Adafruit_MQTT_SPARK.h"
#include "Adafruit_MQTT/Adafruit_MQTT.h"
#include "credentials.h"
const int OLED_RESET = -1;
const int MOISTUREPIN = A1;
const int AQPIN = A0;
const int DUSTPIN = D4;
const int PUMPPIN = D3;
const int OLEDADDRESS = 0x3C;
const int BMEADDRESS = 0x76;
const int SAMPLETIME = 30000;
const int DEGREE = 0xF8;
const int MOISTURETHRESHOLD = 2000;
const int PUMPTIMERMILLIS = 1800000;
const int PUMPTIME = 500;
unsigned int lowPulseOccupancy;
unsigned int lastLPO;
unsigned int duration;
float tempC;                      
float tempF;
float pressPA;
float pressHG;
float humidRH;
float concentration;
int moisture;
int currentTime, lastTime1, lastTime2;
int current_quality =-1;
bool buttonValue;
bool buttonOn;
AirQualitySensor aqSensor(AQPIN);
Adafruit_SSD1306 display(OLED_RESET);
Adafruit_BME280 bme;
IoTTimer checkTimer;
IoTTimer pumpTimer;
TCPClient TheClient;
Adafruit_MQTT_SPARK mqtt(&TheClient,AIO_SERVER,AIO_SERVERPORT,AIO_USERNAME,AIO_KEY); 
Adafruit_MQTT_Publish aqPub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/air-quality");   
Adafruit_MQTT_Publish dustPub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/dust");
Adafruit_MQTT_Publish tempPub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/temperature");
Adafruit_MQTT_Publish humPub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/humidity");
Adafruit_MQTT_Publish pressPub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/pressure");
Adafruit_MQTT_Publish moisturePub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/soil-moisture");
Adafruit_MQTT_Subscribe buttonSub = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/buttononoff");
void MQTT_connect();
bool MQTT_ping();
void publishData();
void displayData();
void runPump();
float getConc(int x);

SYSTEM_MODE(AUTOMATIC);

void setup() {

  Serial.begin();
  waitFor(Serial.isConnected, 10000);
  display.begin(SSD1306_SWITCHCAPVCC, OLEDADDRESS);
  display.clearDisplay();
  display.display();
  mqtt.subscribe(&buttonSub);
  bme.begin(BMEADDRESS);
  aqSensor.init();
  pinMode(MOISTUREPIN, INPUT);
  pinMode(DUSTPIN, INPUT);
  pinMode(PUMPPIN, OUTPUT);
  lowPulseOccupancy = 0;
  lastTime1 = 0;
  lastTime2 = 0;
  checkTimer.startTimer(PUMPTIMERMILLIS);
  buttonOn = true;

}

void loop() {

  MQTT_connect();
  MQTT_ping();
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(100))) {
    if (subscription == &buttonSub) {
      buttonValue = atof((char *)buttonSub.lastread);
    }
  }
  if(buttonValue == 1){
    digitalWrite(PUMPPIN, HIGH);
    buttonOn = true;
  }
  currentTime = millis();
  duration = pulseIn(DUSTPIN, LOW);
  lowPulseOccupancy = lowPulseOccupancy + duration;
  current_quality = aqSensor.slope();
  tempC = bme.readTemperature();
  pressPA = bme.readPressure();
  humidRH = bme.readHumidity();
  tempF = (9.0/5.0)*tempC + 32;
  pressHG = pressPA/3386.0;
  moisture = analogRead(MOISTUREPIN);
  if(buttonValue == 0){
    if(buttonOn == true){
      digitalWrite(PUMPPIN, LOW);
      buttonOn = false;
    }
    if(checkTimer.isTimerReady()){
      if(moisture > MOISTURETHRESHOLD){
        digitalWrite(PUMPPIN, HIGH);
        pumpTimer.startTimer(PUMPTIME);
      }
      checkTimer.startTimer(PUMPTIMERMILLIS);
    }
    if(pumpTimer.isTimerReady()){
      digitalWrite(PUMPPIN, LOW);
    }  
    if(millis() - lastTime1 > SAMPLETIME){    
      if(lowPulseOccupancy !=0){
        lastLPO = lowPulseOccupancy;
      }                                        
      if(lowPulseOccupancy == 0){
        lowPulseOccupancy = lastLPO;
      }
      concentration = getConc(lowPulseOccupancy);
      lastTime1 = millis();
      lowPulseOccupancy = 0;
      publishData();
    }
    if(millis() - lastTime2 > 5000){                                                  
      displayData();
      lastTime2 = millis();
    }
  }

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
void publishData(){
  aqPub.publish(current_quality);
  Serial.printf("Publishing AQ %i \n", current_quality);
  dustPub.publish(concentration);
  Serial.printf("Publishing dust conc %0.2f\n", concentration); 
  tempPub.publish(tempF);
  Serial.printf("Publishing temp %0.2fF\n", tempF);
  pressPub.publish(pressHG);
  Serial.printf("Publishing pressure %0.2f inHG\n", pressHG);
  humPub.publish(humidRH);
  Serial.printf("Publishing humidity %0.2f%%\n", humidRH);
  moisturePub.publish(moisture);
  Serial.printf("Publishing moisture %i\n", moisture);
  Serial.printf("Button value is %i\n", buttonValue);
}
void displayData(){
  if(moisture < MOISTURETHRESHOLD){
    if(current_quality == 3){
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(0, 10);
      display.printf("Soil Moisture: Good\nTemp: %0.2f%cF\nHum: %0.2f%%\nPress: %0.2finHG\nDust: %0.2f\nAir: Fresh", tempF, DEGREE, humidRH, pressHG, concentration);
      display.display();
    }
    if(current_quality == 2){
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(0, 10);
      display.printf("Soil Moisture: Good\nTemp: %0.2f%cF\nHum: %0.2f%%\nPress: %0.2finHG\nDust: %0.2f\nAir: Good", tempF, DEGREE, humidRH, pressHG, concentration);
      display.display();
    }
    if(current_quality == 1){
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(0, 10);
      display.printf("Soil Moisture: Good\nTemp: %0.2f%cF\nHum: %0.2f%%\nPress: %0.2finHG\nDust: %0.2f\nAir: Poor", tempF, DEGREE, humidRH, pressHG, concentration);
      display.display();
    }
    if(current_quality == 0){
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(0, 10);
      display.printf("Soil Moisture: Good\nTemp: %0.2f%cF\nHum: %0.2f%%\nPress: %0.2finHG\nDust: %0.2f\nAir: Bad", tempF, DEGREE, humidRH, pressHG, concentration);
      display.display();
    }
  }
  if(moisture >= MOISTURETHRESHOLD){
    if(current_quality == 3){
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(0, 10);
      display.printf("Soil Moisture: Dry\nTemp: %0.2f%cF\nHum: %0.2f%%\nPress: %0.2finHG\nDust: %0.2f\nAir: Fresh", tempF, DEGREE, humidRH, pressHG, concentration);
      display.display();
    }
    if(current_quality == 2){
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(0, 10);
      display.printf("Soil Moisture: Dry\nTemp: %0.2f%cF\nHum: %0.2f%%\nPress: %0.2finHG\nDust: %0.2f\nAir: Good", tempF, DEGREE, humidRH, pressHG, concentration);
      display.display();
    }
    if(current_quality == 1){
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(0, 10);
      display.printf("Soil Moisture: Dry\nTemp: %0.2f%cF\nHum: %0.2f%%\nPress: %0.2finHG\nDust: %0.2f\nAir: Poor", tempF, DEGREE, humidRH, pressHG, concentration);
      display.display();
    }
    if(current_quality == 0){
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(0, 10);
      display.printf("Soil Moisture: Dry\nTemp: %0.2f%cF\nHum: %0.2f%%\nPress: %0.2finHG\nDust: %0.2f\nAir: Bad", tempF, DEGREE, humidRH, pressHG, concentration);
      display.display();
    }
  }
}
void runPump(){
  digitalWrite(PUMPPIN, HIGH);
}
float getConc(int x){
  float result;
  float ratio;
  ratio = x/(SAMPLETIME*10.0);
  result = 1.1*pow(ratio, 3) - 3.8*pow(ratio, 2) + 520*ratio + 0.62;
  return result;
}