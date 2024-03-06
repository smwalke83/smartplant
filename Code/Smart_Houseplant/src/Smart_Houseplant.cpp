/* 
 * Project Smart Houseplant
 * Author: Scott Walker
 * Date: 2/23/24
 */

#include "Particle.h"                                                   // Basic Header Files
#include "IotClassroom_CNM.h"
#include "Adafruit_GFX.h"                                               // OLED Library
#include "Adafruit_SSD1306.h"                                           // OLED Library
#include "Adafruit_BME280.h"                                            // BME280 Library
#include "math.h"
#include "Air_Quality_Sensor.h"                                         // AQ Sensor Library
#include "Adafruit_MQTT.h"                                              // Lines 14-16 Adafruit Subscribe/Publish Library
#include "Adafruit_MQTT/Adafruit_MQTT_SPARK.h"
#include "Adafruit_MQTT/Adafruit_MQTT.h"
#include "credentials.h"                                                // Adafruit credentials
const int OLED_RESET = -1;                                              // Used for OLED setup
const int MOISTUREPIN = A1;                                             // Lines 19-22 Defining Pins
const int AQPIN = A0;
const int DUSTPIN = D4;
const int PUMPPIN = D3;
const int OLEDADDRESS = 0x3C;                                           // Lines 23-24 Defining serial addresses
const int BMEADDRESS = 0x76;
const int SAMPLETIME = 30000;                                           // 30 Second sample collection time, dust sensor
const int DEGREE = 0xF8;                                                // Hex for ASCII degree symbol
const int MOISTURETHRESHOLD = 2000;                                     // Moisture value at which soil is "dry"
const int PUMPTIMERMILLIS = 1800000;                                    // How often to check if the pump needs to run, in millis
const int PUMPTIME = 1000;                                              // How long to run the pump
unsigned int lowPulseOccupancy;                                         // Lines 30-32 Used in data collection, dust sensor
unsigned int lastLPO;
unsigned int duration;
float tempC;                                                            // Lines 33-37 Variables for the BME280
float tempF;
float pressPA;
float pressHG;
float humidRH;
float concentration;                                                    // Final output of dust sensor
int moisture;                                                           // Variable for soil moisture
int currentTime, lastTime1, lastTime2, lastTime3;                       // Used for simple timers
int current_quality =-1;                                                // Final output of AQ Sensor, value from 0-3
bool buttonValue;                                                       // Variable for button input from Adafruit.io
bool buttonOn;                                                          // Used to affect turning pump on/off when Adafruit.io button ison
bool moistureAlert;                                                     // Used so it only alerts me once when the moisture value goes above the threshold                                                           
AirQualitySensor aqSensor(AQPIN);                                       // Creating AQ Sensor Object
Adafruit_SSD1306 display(OLED_RESET);                                   // Creating OLED Display Object
Adafruit_BME280 bme;                                                    // Creating BME280 Object
IoTTimer checkTimer;                                                    // Lines 48-49 Creating Timer Objects
IoTTimer pumpTimer;
TCPClient TheClient;                                                    // Lines 50-59 Creating Objects for Adafruit.io Subscribe/Publish
Adafruit_MQTT_SPARK mqtt(&TheClient,AIO_SERVER,AIO_SERVERPORT,AIO_USERNAME,AIO_KEY); 
Adafruit_MQTT_Publish aqPub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/air-quality");   
Adafruit_MQTT_Publish dustPub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/dust");
Adafruit_MQTT_Publish tempPub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/temperature");
Adafruit_MQTT_Publish humPub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/humidity");
Adafruit_MQTT_Publish pressPub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/pressure");
Adafruit_MQTT_Publish moisturePub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/soil-moisture");
Adafruit_MQTT_Publish emailPub = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/moisture-alert");
Adafruit_MQTT_Subscribe buttonSub = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/buttononoff");
void MQTT_connect();                                                    // Lines 60-61 Adafruit cloud functions to connect and ping the serveroccasionally
bool MQTT_ping();
void publishData();                                                     // Lines 62-62 Functions to push data to the cloud, serial monitor, and OLED Display 
void displayData();
float getConc(int x);                                                   // Function to take dust sensor data and convert it to dust concentration

SYSTEM_MODE(AUTOMATIC);

void setup() {

  Serial.begin();                                                       // Lines 70-71 Set up serial monitor
  waitFor(Serial.isConnected, 10000);
  display.begin(SSD1306_SWITCHCAPVCC, OLEDADDRESS);                     // Lines 72-74 Set up OLED display
  display.clearDisplay();
  display.display();
  mqtt.subscribe(&buttonSub);                                           // Set up Adafruit subscription
  bme.begin(BMEADDRESS);                                                // Set up BME280
  aqSensor.init();                                                      // Set up AQ Sensor
  pinMode(MOISTUREPIN, INPUT);                                          // Lines 78-80 Pin Modes for components that need it
  pinMode(DUSTPIN, INPUT);
  pinMode(PUMPPIN, OUTPUT);
  checkTimer.startTimer(PUMPTIMERMILLIS);                               // Starting the timer to check the soil
  lowPulseOccupancy = 0;                                                // Lines 82-86 Predefining some variables to their starting values
  lastTime1 = 0;
  lastTime2 = 0;
  buttonOn = true;
  moistureAlert = false;

}

void loop() {

  MQTT_connect();                                                       // Connect to Adafruit MQTT
  MQTT_ping();                                                          // Occasionally ping the server
  Adafruit_MQTT_Subscribe *subscription;                                // Lines 94-99 Check subscription for changes (button press)
  while ((subscription = mqtt.readSubscription(100))) {
    if (subscription == &buttonSub) {
      buttonValue = atof((char *)buttonSub.lastread);
    }
  }
  if(buttonValue == 1){                                                 // Lines 100-103 If the button (on Adafruit dashboard) is on, run the pump
    digitalWrite(PUMPPIN, HIGH);
    buttonOn = true;
  }
  currentTime = millis();                                               // Recording time since startup
  duration = pulseIn(DUSTPIN, LOW);                                     // Lines 105-106 Data collection for dust sensor
  lowPulseOccupancy = lowPulseOccupancy + duration; 
  current_quality = aqSensor.slope();                                   // Obtaining Air Quality
  tempC = bme.readTemperature();                                        // Lines 108-110 Obtaining readings from BME280
  pressPA = bme.readPressure();
  humidRH = bme.readHumidity();
  tempF = (9.0/5.0)*tempC + 32;                                         // Lines 111-112 Converting BME data to preferred units
  pressHG = pressPA/3386.0;
  moisture = analogRead(MOISTUREPIN);                                   // Obtaining soil moisture level
  if(buttonValue == 0){                                                 // If button (on Adafruit dashboard) is off...
    if(buttonOn == true){                                                 // Lines 115-118 Turn pump off if it was on
      digitalWrite(PUMPPIN, LOW);
      buttonOn = false;
    }
    if(checkTimer.isTimerReady()){                                        // If timer to check soil is ready... 
      if(moisture > MOISTURETHRESHOLD){                                      // Lines 120-125 If soil is dry, start pump and pump timer, restart timer to check soil
        digitalWrite(PUMPPIN, HIGH);
        pumpTimer.startTimer(PUMPTIME);
      }
      checkTimer.startTimer(PUMPTIMERMILLIS);
    }
    if(pumpTimer.isTimerReady()){                                         // Lines 126-128 Turn off pump once pump timer is ready
      digitalWrite(PUMPPIN, LOW);
    }  
    if(millis() - lastTime1 > SAMPLETIME){                                // If 30 seconds has passed...
      if(lowPulseOccupancy !=0){                                            // Lines 130-132 If dust sensor returned a value, save it in case it returns zero later
        lastLPO = lowPulseOccupancy;
      }                                        
      if(lowPulseOccupancy == 0){                                           // Lines 133-135 If dust sensor returned zero, set value to last non-zero reading
        lowPulseOccupancy = lastLPO;
      }
      concentration = getConc(lowPulseOccupancy);                           // Convert dust sensor data to dust concentration
      lastTime1 = millis();                                                 // Reset 30 second timer
      lowPulseOccupancy = 0;                                                // Reset dust sensor variable
      publishData();                                                        // Push data to cloud
    }
    if(millis() - lastTime2 > 5000){                                      // Lines 141-144 If 5 seconds have passed refresh data on OLED display, reset 5 second timer
      displayData();
      lastTime2 = millis();
    }
    if(moisture > MOISTURETHRESHOLD && moistureAlert == false){           // Lines 145-151 The first time moisture levels go above the threshold, publish to Adafruit (Zapier will send only one email this way) - Resets when value drops below threshold again  
      emailPub.publish(moisture);
      moistureAlert = true;
    }
    if(moisture < MOISTURETHRESHOLD && moistureAlert == true){
      moistureAlert = false;
    }
  }
}
bool MQTT_ping() {                                                        // The function to ping the cloud server. Not written by me.
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
void MQTT_connect() {                                                   // The function to connect to the cloud server. Not written by me.
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
void publishData(){                                                     // The function to push environmental data to the cloud, as well as printing it to the serial monitor.
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
  Serial.printf("Button value is %i\n", buttonValue);                     // Also publishes the value from the Adafruit subscription
}
void displayData(){                                                     // The function to populate the OLED display. Mostly prints raw data, although it interprets the air quality and soil moiture for you.
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
float getConc(int x){                                                   // This function takes the data gathered by the dust sensor and converts it to dust concentration.
  float result;
  float ratio;
  ratio = x/(SAMPLETIME*10.0);
  result = 1.1*pow(ratio, 3) - 3.8*pow(ratio, 2) + 520*ratio + 0.62;
  return result;
}