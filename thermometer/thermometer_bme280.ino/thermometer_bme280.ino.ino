/**
 * Arduino nano + Ethernet shild enc28j60 + sensor BME280
 * Author: Martin Korbel
 * Version: 1.0
 */
#define MACADDRESS 0x0a,0xa1,0xa2,0xa3,0xa4,0xa7
#define PERIOD 300000
#define NAME "outTherm01"
#define DEBUG false
#include <Wire.h>
#include <UIPEthernet.h>    
/* 
 *  Disable UDP protocol in UIPEthernet
 *  sed -i 's/^#define UIP_CONF_UDP  .*$/#define UIP_CONF_UDP  0/' libraries/UIPEthernet/utility/uipethernet-conf.h
 *  
 */
#include <PubSubClient.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#define SEALEVELPRESSURE_HPA (1013.25) // (1030.16)

Adafruit_BME280 bme; 

// Network
uint8_t mac[6] = {MACADDRESS};
IPAddress ip(192, 168, 77,  55);
IPAddress server(192, 168, 77,  10);
EthernetClient ethClient;
PubSubClient mqttClient(server, 1883, ethClient);
unsigned long next;


void reconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    #if DEBUG
    Serial.print("Attempting MQTT connection...");    
    #endif
    if (mqttClient.connect(NAME)) {
      #if DEBUG
      Serial.println("reconnected");
      #endif
      mqttClient.publish(NAME"/status","reconnected");           
    } else {
      #if DEBUG
      Serial.print("failed, rc=");  
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      #endif
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() 
{
  #if DEBUG
  Serial.begin(9600); 
  #endif
  bool status;     
  status = bme.begin(0x76);  
  while (!status) {    
      #if DEBUG
      Serial.println("Could not find a valid BME280 sensor, check wiring!");
      #endif
      status = bme.begin(0x76);  
      delay(3000);
  }
  Ethernet.begin(mac, ip) ;    

  // Start MQTT  
  ethClient.connect(server, 1833);
  if (mqttClient.connect(NAME)) {
      #if DEBUG
      Serial.println("Thermometer is online");
      Serial.println(Ethernet.localIP());      
      #endif
      mqttClient.publish(NAME"/status","online");      
  }
  next = millis() + PERIOD;
}

void loop() 
{    
  if (millis() - next > PERIOD) {  
    if (!mqttClient.connected()) {
      reconnect();
    }          
    static char b[5];
    #if DEBUG
    Serial.println(dtostrf(bme.readTemperature(), 5, 2, b));
    #endif
    mqttClient.publish(NAME"/temperature", dtostrf(bme.readTemperature(), 5, 2, b));    // C
    mqttClient.publish(NAME"/presure", dtostrf(bme.readPressure() / 100.0F, 5, 2, b));  // hPa
    mqttClient.publish(NAME"/humidity", dtostrf(bme.readHumidity(), 5, 2, b));          // %    
    next = millis();
  }
  mqttClient.loop();
 // delay(PERIOD);  
}
