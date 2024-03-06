/* 
 * Project Zapier Unread Email Alert
 * Author: Scott Walker
 * Date: 3/5/2024
 */

#include "Particle.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT/Adafruit_MQTT_SPARK.h"
#include "Adafruit_MQTT/Adafruit_MQTT.h"
#include "credentials.h"
const int LEDPIN = D7;
int currentTime;
int lastTime1;
int lastTime2;
bool ledOn;
TCPClient TheClient;
Adafruit_MQTT_SPARK mqtt(&TheClient,AIO_SERVER,AIO_SERVERPORT,AIO_USERNAME,AIO_KEY);
Adafruit_MQTT_Subscribe emailSub = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/cnm-email");
void MQTT_connect();
bool MQTT_ping();

SYSTEM_MODE(AUTOMATIC);

void setup() {

  Serial.begin();
  waitFor(Serial.isConnected, 10000);
  mqtt.subscribe(&emailSub);
  pinMode(LEDPIN, OUTPUT);
  lastTime2 = -99999;

}

void loop() {

  MQTT_connect();
  MQTT_ping();
  currentTime = millis();
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(100))) {
    if (subscription == &emailSub) {
      Serial.printf("New Message!\n");
      digitalWrite(LEDPIN, HIGH);
      lastTime1 = millis();
      ledOn = true;
    }
  }
  if(ledOn){
    if(currentTime - lastTime1 > 30000){
      digitalWrite(LEDPIN, LOW);
      ledOn = false;
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