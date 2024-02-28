//################# LIBRARIES ################

#include <WebServer_WT32_ETH01.h>                                                         // Standard ESP32 Arduino library used for MQTT
#include <PubSubClient.h>                                                                 // Standard ESP32 Arduino library used for MQTT
#include <Wire.h>                                                                         // Standard ESP32 Arduino library for TWE/I2C bus
#include <SPI.h>                                                                          // Standard ESP32 Arduino library for SPI bus
#include <Time.h>                                                                         // Standard ESP32 Arduino library
#include <TimeAlarms.h>                                                                   // Standard ESP32 Arduino library
#include "MCP23S08.h"                                                                     // library from https://urldefense.com/v3/__https://github.com/julianschuler/MCP23S08__;!!AcCyiFYNC0XOnw!jbcd5AeVNrmmFtmLXER4SJWqreVNNO_dh3wvCr2T5CMJ8Mr5AS8-vw94Tt3JqMSLGUwTBLHy43z5vpxHzUmTfO7Qm3c$ 
#include <SparkFunBME280.h>                                                               // from Sparkfun under BSD License to be found in Arduino library install

// i2c wire.h normally used i2c pins are not availeble on WT32-ETH01 so use other pins for i2c
#define I2C_SCL    32    // WT32-ETH01 CFG    = Gpio 32      non standard i2c adress 
#define I2C_SDA    33    // WT32-ETH01 485_EN = Gpio 33      non standard i2c adress

// Expander Pin Assignments
#define LIGHT230VAC_PIN 0
#define WALLSOCKET230VAC_PIN 1 
#define SPAREOUTPUT_PIN 2
#define DOORUP_PIN 4
#define DOORDOWN_PIN 5
#define SPARE_DI6_PIN 6

// SPI bus defines
#define SCLK 14
#define MISO 2 
#define MOSI 15
#define CS_PIN_MCP23S08 4

// DC Motor defines
#define MOTOR_LEFT 5
#define MOTOR_RIGHT 17

// Booleans
bool cooplight = false;
bool coopdoor = false;                                    // False is down
bool coopdoorrunning = false;
bool doorup = false, doordown = false;            
bool dooruppin = false, doordownpin = false;
bool sparepin6 = false;

// MCP23S08 IO expander
MCP23S08 expander(CS_PIN_MCP23S08);

// BME280 sensors
BME280 BMEsensorCoop;

WebServer server(80);

// Select the IP address according to your local network
IPAddress myIP(192, 168, 1, 36);
IPAddress myGW(192, 168, 1, 1);
IPAddress mySN(255, 255, 255, 0);
IPAddress myDNS(8, 8, 8, 8);                                // Google DNS Server IP
const char *mqttServer = "192.168.1.11";                    // Broker address
const char *ID        = "ESP32_Chickencoop_Controller";     // Name of our device, must be unique
const char *TOPIC     = "ESP32_Chickencoop";                // Topic to subcribe to
const char *subTopic  = "ESP32_Chickencoop";                // Topic to subcribe to

void callback(char* topic, byte* payload, unsigned int length) 
{
}

WiFiClient    ethClient;
PubSubClient    client(mqttServer, 1883, callback, ethClient);

// NTP Server
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;
struct tm timeinfo;

void setup()
{
// MQTT Client setup  
  // To be called before ETH.begin()
  WT32_ETH01_onEvent();

  //bool begin(uint8_t phy_addr=ETH_PHY_ADDR, int power=ETH_PHY_POWER, int mdc=ETH_PHY_MDC, int mdio=ETH_PHY_MDIO, 
  //           eth_phy_type_t type=ETH_PHY_TYPE, eth_clock_mode_t clk_mode=ETH_CLK_MODE);
  //ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_TYPE, ETH_CLK_MODE);
  ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER);

  // Static IP, leave without this line to get IP via DHCP
  //bool config(IPAddress local_ip, IPAddress gateway, IPAddress subnet, IPAddress dns1 = 0, IPAddress dns2 = 0);
  ETH.config(myIP, myGW, mySN, myDNS);

  WT32_ETH01_waitForConnect();

  server.on(F("/"), handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();

  client.setServer(mqttServer, 1883);
  client.setCallback(callback);

  Wire.begin(I2C_SDA, I2C_SCL);                                                           // start up the I2C bus (BME280 sensors)                
  SPI.begin(SCLK, MISO, MOSI);                                                            // start up the SPI bus (MCP23S08 and MS5525DSO)

  // Setup MCP23S08 IO Expander
  expander.begin();                                                                       // begin communication with the pin/O expander
  for (uint8_t pin = 0; pin < 4; pin++) {                                                 // pins 0 - 3 available
    expander.pinModeIO(pin, OUTPUT);                                                      // set pins to output
  }
  for (uint8_t pin = 4; pin < 8; pin++) {                                                 // pins 4 - 7 available
    expander.pinModeIO(pin, INPUT);                                                       // set pins to input
  }

  // Setup BME280 sensor
  BMEsensorCoop.setI2CAddress(0x76);
  
  BMEsensorCoop.beginI2C();
  
  BMEsensorCoop.setFilter(0);                                                              //0 to 4 is valid. Filter coefficient. See 3.4.4
  BMEsensorCoop.setStandbyTime(5);                                                         //0 to 7 valid. Time between readings. See table 27.

  BMEsensorCoop.setTempOverSample(16);                                                      //0 to 16 are valid. 0 disables temp sensing. See table 24.
  BMEsensorCoop.setPressureOverSample(16);                                                  //0 to 16 are valid. 0 disables pressure sensing. See table 23.
  BMEsensorCoop.setHumidityOverSample(16);                                                  //0 to 16 are valid. 0 disables humidity sensing. See table 19.
  
  BMEsensorCoop.setMode(MODE_NORMAL);                                                      //MODE_SLEEP, MODE_FORCED, MODE_NORMAL is valid. See 3.3

  // DC motor pins setup
  pinMode(MOTOR_LEFT, OUTPUT);                                                            // set pin 7 from ESP for left turning of DC motor
  pinMode(MOTOR_RIGHT, OUTPUT);                                                           // set pin 8 from ESP for right turning of DC motor
  // Set outputs to LOW
  digitalWrite(MOTOR_LEFT, LOW);
  digitalWrite(MOTOR_RIGHT, LOW);

  //set real time clock
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);  
  getLocalTime(&timeinfo);
   
  Alarm.timerRepeat(0,15,0, runalarm_15min);
//  Alarm.timerRepeat(0,0,10, toggle_10sec);

  // Allow the hardware to sort itself out
  delay(1500);

  expander.digitalWriteIO(WALLSOCKET230VAC_PIN, HIGH);
}

char tempString[8];
char timeStringBuff[50];
  
void loop() 
{
  if (!coopdoorrunning) {
    if (!client.connected()) 
    {
      reconnect();
    }
  
    Alarm.delay(0);                                                                                 // call for timealarms to function
  
    server.handleClient();

    client.loop();
  }

  if ((timeinfo.tm_hour >= 16) && (timeinfo.tm_hour <= 20) && !cooplight) {
    cooplight=true;
    expander.digitalWriteIO(LIGHT230VAC_PIN, HIGH);
    client.publish("ESP32_Chickencoop/LightChickencoop", "1");
  }
  else if (((timeinfo.tm_hour < 16) || (timeinfo.tm_hour > 20)) && cooplight) {
    cooplight=false;
    expander.digitalWriteIO(LIGHT230VAC_PIN, LOW);
    client.publish("ESP32_Chickencoop/LightChickencoop", "0");
  }

  dooruppin = expander.digitalReadIO(DOORUP_PIN);
  doordownpin = expander.digitalReadIO(DOORDOWN_PIN);

  if ((timeinfo.tm_hour >= 7) && (timeinfo.tm_hour <= 18) && !coopdoor && !dooruppin && !coopdoorrunning) {
//  if ((timeinfo.tm_sec >= 30) && !coopdoor && !dooruppin && !coopdoorrunning) {
    coopdoor=true;
    digitalWrite(MOTOR_LEFT, HIGH);
//    client.publish("ESP32_Chickencoop/DoorChickencoopMotorleft", "1");
    coopdoorrunning=true;
  }
  else if (((timeinfo.tm_hour < 7) || (timeinfo.tm_hour > 18)) && coopdoor && !doordownpin && !coopdoorrunning) {
//  else if ((timeinfo.tm_sec < 30) && coopdoor && !doordownpin && !coopdoorrunning) {
    coopdoor=false;
    digitalWrite(MOTOR_RIGHT, HIGH);
//    client.publish("ESP32_Chickencoop/DoorChickencoopMotorright", "1");
    coopdoorrunning=true;
  }

  if ( coopdoorrunning && dooruppin && coopdoor) {
    digitalWrite(MOTOR_LEFT, LOW);
//    client.publish("ESP32_Chickencoop/DoorChickencoopMotorleft", "0");
   	client.publish("ESP32_Chickencoop/DoorChickencoop", "1");
    coopdoorrunning=false;
  }
  if ( coopdoorrunning && doordownpin && !coopdoor) {
    digitalWrite(MOTOR_RIGHT, LOW);
//    client.publish("ESP32_Chickencoop/DoorChickencoopMotorright", "0");
	  client.publish("ESP32_Chickencoop/DoorChickencoop", "0");
    coopdoorrunning=false;
  }
}

void runalarm_15min(){
  getLocalTime(&timeinfo);
  strftime(timeStringBuff, sizeof(timeStringBuff), "%A, %B %d %Y %H:%M:%S", &timeinfo);
  client.publish("ESP32_Chickencoop/Debug/Time", timeStringBuff);
  
  dtostrf(BMEsensorCoop.readTempC(), 1, 1, tempString);
  client.publish("ESP32_Chickencoop/TemperatureChickencoop", tempString);
  dtostrf(BMEsensorCoop.readFloatHumidity(), 1, 0, tempString);
  client.publish("ESP32_Chickencoop/HumidityChickencoop", tempString);
  dtostrf((BMEsensorCoop.readFloatPressure() / 100.0F), 1, 0, tempString);
  client.publish("ESP32_Chickencoop/PressureChickencoop", tempString); 
}

void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    // Attempt to connect
    if (client.connect(ID, "try", "try"))
    {
      // This is a workaround to address https://urldefense.com/v3/__https://github.com/OPEnSLab-OSU/SSLClient/issues/9__;!!AcCyiFYNC0XOnw!jbcd5AeVNrmmFtmLXER4SJWqreVNNO_dh3wvCr2T5CMJ8Mr5AS8-vw94Tt3JqMSLGUwTBLHy43z5vpxHzUmTGxF6hEo$ 
      //ethClientSSL.flush();
      // ... and resubscribe
      client.subscribe(subTopic);
      // for loopback testing
      client.subscribe(TOPIC);
      // This is a workaround to address https://urldefense.com/v3/__https://github.com/OPEnSLab-OSU/SSLClient/issues/9__;!!AcCyiFYNC0XOnw!jbcd5AeVNrmmFtmLXER4SJWqreVNNO_dh3wvCr2T5CMJ8Mr5AS8-vw94Tt3JqMSLGUwTBLHy43z5vpxHzUmTGxF6hEo$ 
      //ethClientSSL.flush();
    }
    else
    {
      // Wait 5 seconds before retrying 
      delay(5000);
    }
  }
}

void handleRoot()
{
#define BUFFER_SIZE     1000
  char tempTempC[8],tempHum[8],tempPress[8];
  char temp[BUFFER_SIZE];
  char templight[4], tempdoor[4], tempdooruppin[4], tempdoordownpin[4];
  getLocalTime(&timeinfo);
  strftime(timeStringBuff, sizeof(timeStringBuff), "%A, %B %d %Y %H:%M:%S", &timeinfo);
  dtostrf(BMEsensorCoop.readTempC(), 1, 1, tempTempC);
  dtostrf(BMEsensorCoop.readFloatHumidity(), 1, 0, tempHum);
  dtostrf((BMEsensorCoop.readFloatPressure() / 100.0F), 1, 0, tempPress);

  if (cooplight) {
    strcpy(templight, "ON");
  }
  else {
    strcpy(templight, "OFF");
  }

  if (coopdoor) {
    strcpy(tempdoor, "ON");
  }
  else {
    strcpy(tempdoor, "OFF");
  }

  if (dooruppin) {
    strcpy(tempdooruppin, "ON");
  }
  else {
    strcpy(tempdooruppin, "OFF");
  }

  if (doordownpin) {
    strcpy(tempdoordownpin, "ON");
  }
  else {
    strcpy(tempdoordownpin, "OFF");
  }

  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;
  int day = hr / 24;

  hr = hr % 24;

  snprintf(temp, BUFFER_SIZE - 1,
           "<html>\
<head>\
<meta http-equiv='refresh' content='5'/>\
<title>ChickenCoop Controller</title>\
<style>\
body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; text-align: center; }\
</style>\
</head>\
<body>\
<h2>ChickenCoop Status</h2>\
<p>Uptime: %d d %02d:%02d:%02d</p>\
<p>Actual time: %s</p>\
<p>ChickenCoop Humidity: %s</p>\
<p>ChickenCoop Pressure: %s</p>\
<p>ChickenCoop Temperature(C): %s</p>\
<p>ChickenCoop Lights %s</p>\
<p>ChickenCoop Door %s</p>\
<p>ChickenCoop Doorsensor up %s</p>\
<p>ChickenCoop Doorsensor down %s</p>\
</body>\
</html>", day, hr % 24, min % 60, sec % 60, timeStringBuff,tempHum,tempPress,tempTempC,templight,tempdoor, tempdooruppin, tempdoordownpin);

  server.send(200, F("text/html"), temp);
}

void handleNotFound()
{
  String message = F("File Not Found\n\n");

  message += F("URI: ");
  message += server.uri();
  message += F("\nMethod: ");
  message += (server.method() == HTTP_GET) ? F("GET") : F("POST");
  message += F("\nArguments: ");
  message += server.args();
  message += F("\n");

  for (uint8_t i = 0; i < server.args(); i++)
  {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, F("text/plain"), message);
}

/*
int greenTime = 0;
int redTime = 0;

int timePassed (time) {
  int diff = 0;

  if (millis() <= time) {
    diff = (69666 - time) + millis();
  } else {
    diff = millis() - time;
  }

  return diff;
}

void loop() {
  if (timePassed (greenTime) >= 2000) {
    switchGreen();
    greenTime = millis();
  }

  if (timePassed (redTime) >= 1000) {
    switchRed();
    redTime = millis();
  }
}
*/