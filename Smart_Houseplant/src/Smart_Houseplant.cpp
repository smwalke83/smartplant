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
unsigned int lowPulseOccupancy;
unsigned int duration;
float tempC;                      
float tempF;
float pressPA;
float pressHG;
float humidRH;
float ratio;
float concentration;
int moisture;
int currentTime, lastTime1, lastTime2;
int current_quality =-1;
AirQualitySensor aqSensor(AQPIN);
Adafruit_SSD1306 display(OLED_RESET);
Adafruit_BME280 bme;
IoTTimer pumpTimer;
TCPClient TheClient;
Adafruit_MQTT_SPARK mqtt(&TheClient,AIO_SERVER,AIO_SERVERPORT,AIO_USERNAME,AIO_KEY); 
// Adafruit_MQTT_Publish aqPub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/air-quality");   This needs to change to the feed/s everything will be published to
void MQTT_connect();
bool MQTT_ping();

SYSTEM_MODE(AUTOMATIC);

void setup() {

  Serial.begin();
  waitFor(Serial.isConnected, 10000);
  display.begin(SSD1306_SWITCHCAPVCC, OLEDADDRESS);
  display.clearDisplay();
  display.display();
  bme.begin(BMEADDRESS);
  aqSensor.init();
  pinMode(MOISTUREPIN, INPUT);
  pinMode(DUSTPIN, INPUT);
  pinMode(PUMPPIN, OUTPUT);
  lowPulseOccupancy = 0;
  lastTime1 = 0;
  lastTime2 = 0;

}

void loop() {

  currentTime = millis();
  duration = pulseIn(DUSTPIN, LOW);
  lowPulseOccupancy = lowPulseOccupancy + duration;
  current_quality = aqSensor.slope();
  tempC = bme.readTemperature();
  pressPA = bme.readPressure();
  humidRH = bme.readHumidity();
  tempF = (9.0/5.0)*tempC + 32;
  pressHG = pressPA/3386.0;
  if(millis() - lastTime1 > SAMPLETIME){                                            // dust sensor values
    ratio = lowPulseOccupancy/(SAMPLETIME*10.0);
    concentration = 1.1*pow(ratio, 3) - 3.8*pow(ratio, 2) + 520*ratio + 0.62;
    Serial.printf("Ratio:%0.2f, Concentration:%0.2f\n", ratio, concentration);
    lastTime1 = millis();
    lowPulseOccupancy = 0;
  }
  if(millis() - lastTime2 > 5000){                                                  // AQ Sensor values
    if (current_quality >= 0){
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
    }
    moisture = analogRead(MOISTUREPIN);                                             // soil moisture
    Serial.printf("Moisture is %d\n", moisture);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.printf("Soil Moisture: %d\nTemp: %d\nHum: %dPress: %d\nDust: %d\nAir: %d", moisture, tempF, humidRH, pressHG, concentration, current_quality)
    lastTime2 = millis();
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