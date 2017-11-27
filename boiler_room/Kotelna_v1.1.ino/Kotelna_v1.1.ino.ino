/**
 * This is an automations module for my boiler room. The logic is implemented by node-red on server.
 *  
 *  * MQTT communucations
 *   - "outKotelna/online"   - by inicializing of device, send number of sensors
 *   - "outKotelna/sensors/<ID>"   - every minute send temperature (C), ID is unique sensor ID
 *   - "outKotelna"          - confirmation message, value is "PIN22 is OFF" or "PIN22 is ON"
 *   - "outKotelna/offline"  - information about timeout of server 
 *   - "inKotelna/<num>"     - switch on/off the pin (relay), num is number of pin and value is 1/0
 *                             after switching it sends confirmation about that.
 *   - "inKotelna/NOOP"      - heart beating, the module requires less than every 15min any signal.
 *                             If it does not have, switch off all relays (server is offline) and 
 *                             sends information message about that.
 *    
 *  * Devices:
 *   Arduino MEGA 2560
 *   Ethernet shield W5100 
 *   8 Relay module (PINs: 22,24,26,28,30,32,36)
 *   Oled display 128x64 SDA/SCL (PINs: 20,21)
 *   5x sensores DS18B20 1-wire (PINs: 2)
 *   
 *   
 *  * Display shows temperature of first four sensors and the indicator of enabled relays.
 *    0: 24.00°C         0 | 1
 *    1: 40.00°C         2 | 
 *    2: 50.00°C           |
 *    3" 60.00°C           | 7
 *   
 * Author: Martin Korbel arduino@blackserver.cz
 * Version: 1.1  (2017-11-22)
 */
//#define DEBUG 1
#define HIGHX 0x0
#define LOWX 0x1
#define ONEW_PIN 2
#define TEMPERATURE_PRECISION 9 
#define MQTT_PLACE "Kotelna"  // Topic
#define MQTT_SERVER_IP 192,168,77,10
#define MQTT_SERVER_PORT 1883
#define MQTT_TIMEOUT 900000   // 15min
#define MQTT_SENSOR_PERIOD 60000 // 60s
#define IPADDRESS 192,168,77,50
#define IPMASK 255,255,255,0
#define IPDNS 192,168,77,1
#define IPGW 192,168,77,1
#define MACADDRESS 0x00,0x01,0x02,0x03,0x04,0x09
#define DISPLAY_PERIOD 300       // 500m
// MQTT
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>

// Display
#include <U8g2lib.h>
#include <U8x8lib.h>

// temperature
#include <OneWire.h>
#include <DallasTemperature.h>


OneWire oneWireDS(ONEW_PIN);
DallasTemperature senzoryDS(&oneWireDS);


// Callback function header
void callback(char* topic, byte* payload, unsigned int length);

// MQTT
EthernetClient ethClient;
PubSubClient client({ MQTT_SERVER_IP }, MQTT_SERVER_PORT, callback, ethClient);

// Display
U8G2_SSD1306_128X64_NONAME_F_SW_I2C oled(U8G2_R0, /* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE);

// Variable for relay switch
char switchOn[8] = {'0','0','0','0','0','0','0','0'};

// Variable for reload display
unsigned long int reload_display_int = 0;

// Variable for reload temp
unsigned long int reload_temp_int = 0;

// Variable for MQTT timeout
unsigned long int reload_mqtt_int = 0;

// Inicialize sernsors
void initDS1882() 
{    
  senzoryDS.begin();      
  byte numDS = senzoryDS.getDeviceCount();  
  #ifdef DEBUG 
  Serial.print("Inicialization DS1882 - ");    
  Serial.print(numDS,  DEC);
  Serial.println(" devices");    
  Serial.print("Parasite power is: "); 
  if (senzoryDS.isParasitePowerMode()) Serial.println("ON");
  else Serial.println("OFF");  
  #endif
  
  for(int i = 0; i < numDS; i++) {
    DeviceAddress tempDeviceAddress;
    senzoryDS.getAddress(tempDeviceAddress, i);
    senzoryDS.setResolution(tempDeviceAddress, TEMPERATURE_PRECISION);
    #ifdef DEBUG
    if (!senzoryDS.validAddress(tempDeviceAddress)) {
      Serial.println("This is not valid address");
    }
    Serial.print(i);
    Serial.print(" = ");
    char a1[17];    
    sprintf(a1, "%02X%02X%02X%02X%02X%02X%02X%02X\0",\
        tempDeviceAddress[0], tempDeviceAddress[1], tempDeviceAddress[2], tempDeviceAddress[3],\
        tempDeviceAddress[4], tempDeviceAddress[5], tempDeviceAddress[6], tempDeviceAddress[7]);    
    Serial.println(a1);
    #endif
  }
}

void process_DS1882() 
{    
  byte numDS = senzoryDS.getDeviceCount();
  #ifdef DEBUG
  Serial.print(numDS,  DEC);
  Serial.println(" devices !!!");  
  #endif
  senzoryDS.requestTemperatures();        
  for(byte i = 0; i < numDS; i++) {  
    DeviceAddress tempDeviceAddress;
    if(senzoryDS.getAddress(tempDeviceAddress, i)) {                    
      char a1[64];          
      char c1[17];                         
      sprintf(c1, "%02X%02X%02X%02X%02X%02X%02X%02X\0",\
              tempDeviceAddress[0], tempDeviceAddress[1], tempDeviceAddress[2], tempDeviceAddress[3],\
              tempDeviceAddress[4], tempDeviceAddress[5], tempDeviceAddress[6], tempDeviceAddress[7]);            
      sprintf(a1, "out" MQTT_PLACE "/sensors/%s\0", c1);
      float temp = senzoryDS.getTempC(tempDeviceAddress);            
      char tempT[7];
      dtostrf(temp, 5, 2, tempT);
      client.publish(a1, tempT);
      #ifdef DEBUG              
        Serial.print(i, DEC);
        Serial.print(" ");
        Serial.print(c1);
        Serial.print(": ");
        if (senzoryDS.validAddress(tempDeviceAddress)) Serial.print("Yes ");
        else Serial.print("NO ");  
        Serial.println(temp);              
      #endif         
    } else {
      #ifdef DEBUG      
        Serial.print(i, DEC);
        Serial.println(" no address");
      #endif
    }
  }  
}

void reload_display() 
{  
  byte numDS = senzoryDS.getDeviceCount();
  for (int i=0; i < numDS && i < 4; i++) {
    char c[4];    
    sprintf(c,"%d:", i);    
    oled.setFont(u8g2_font_helvR08_tf);  
    oled.drawStr(7, 10+(i*18), c);    
    char a[20];
    char b[5];
    sprintf(a, "%s""\xb0""C\0", dtostrf(senzoryDS.getTempCByIndex(i), 5, 2, b));
    oled.setFont(u8g2_font_t0_12_tf);
    oled.drawStr(25, 10+(i*17), a);      
  }
    
  int pos[2] = {95, 111}; 
  oled.drawLine(105, 0, 105, 64);
  for (int j=0; j<8; j++) {
    char pp[2];
    sprintf(pp,"%d\0",j);    
    if (switchOn[j] == '1') {
      #ifdef DEBUG              
        Serial.println(pp);
      #endif
      oled.drawStr(pos[j % 2], 12+(ceil(j/2)*17), pp);
    }    
    if (j % 2 == 0) {
      oled.drawLine(90, 16+(ceil(j/2)*17), 120, 16+(ceil(j/2)*17));
    }
  }    
}

void pinON(int pin) 
{
  digitalWrite(pin, HIGHX);
  char m[20];
  sprintf(m, "PIN%d is ON", pin);
  client.publish("out" MQTT_PLACE "", m);
  switchOn[(pin-22)/2] = '1';
  #ifdef DEBUG        
    Serial.println(m);
  #endif
}

void pinOFF(int pin) 
{
  digitalWrite(pin, LOWX);
  char m[20];
  sprintf(m, "PIN%d is OFF", pin);
  client.publish("out" MQTT_PLACE "", m);
  switchOn[(pin-22)/2] = '0';
  #ifdef DEBUG        
    Serial.println(m);
  #endif
}

// Callback function for MQTT
void callback(char* topic, byte* payload, unsigned int length) 
{ 
  reload_mqtt_int =  millis();
  if ( topic == "in" MQTT_PLACE "/NOOP" ) {        
        return;
  }
  int lightPin = 53;
  int res = sscanf(topic, "in" MQTT_PLACE "/%d", &lightPin);  
  if (res != 1) {
    char msg[] = "Problem with parse pin ";
    client.publish("out" MQTT_PLACE "/error", strcat(msg, topic));
    #ifdef DEBUG        
      Serial.println(strcat(msg, topic));
    #endif
    return;
  } 
  
  //turn the LED ON if the payload is '1' and publish to the MQTT server a confirmation message
  if(payload[0] == '1'){
    pinON(lightPin);
  }
    
  //turn the LED OFF if the payload is '0' and publish to the MQTT server a confirmation message
  if (payload[0] == '0'){
    pinOFF(lightPin);
  }
  
}


void setup() 
{
  #ifdef DEBUG
    Serial.begin(9600);  
  #endif

  
  // Sensors
  initDS1882();  
  
  // Reset PIN 22,24,..,36
  
  int  pin = 22;
  while (pin < 37) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOWX);
    pin += 2;
  }
    
  // Start network
  IPAddress ip(IPADDRESS);
  IPAddress gw(IPGW);
  IPAddress dns(IPDNS);
  IPAddress mask(IPMASK);
  #ifdef DEBUG
    Serial.println(ip);  
  #endif
  uint8_t mac[6] = {MACADDRESS};
  Ethernet.begin(mac, ip, dns, gw, mask);
 
  // Display
  oled.begin();  
  oled.setFlipMode(0);  
  oled.firstPage();
  oled.setFont(u8g2_font_t0_12_tf);
  char ipp[20];
  sprintf(ipp, "%d.%d.%d.%d\0", ip[0],ip[1],ip[2],ip[3]);   
  oled.drawStr(25, 10, ipp);      
  oled.nextPage();
  reload_display_int = millis();  
 
  
  // Start MQTT
  if (client.connect("arduinoClient")) {     
      byte numDS = senzoryDS.getDeviceCount(); 
      char a[16];            
      sprintf(a, "sensors:%d/0", numDS); 
      client.publish("out" MQTT_PLACE "/online", a);
      client.subscribe("in" MQTT_PLACE "/#");
  }
  
  reload_mqtt_int = millis();
}

void loop() 
{  
  // MQTT stuff
  client.loop();
 
  // Display redraw
  if (millis()-reload_display_int > DISPLAY_PERIOD ) {  
    oled.firstPage();
    senzoryDS.requestTemperatures();
    do {
      reload_display();                 
    } while( oled.nextPage() );  
    reload_display_int = millis();
  }  
 
  
  if (millis()-reload_temp_int > MQTT_SENSOR_PERIOD) {                
    process_DS1882();     
    reload_temp_int = millis();
  }  
 
  if (millis()-reload_mqtt_int > MQTT_TIMEOUT) {    
    // If server is not online, all shutdown
    int  pin = 22;
    while (pin < 37) {    
      pinOFF(pin);
      pin += 2;
    } 
    client.publish("out" MQTT_PLACE "/offline", "MQTT server is unavailable");
    #ifdef DEBUG        
      Serial.println("MQTT server is unavailable");
    #endif
    reload_mqtt_int = millis();
  }   
}
