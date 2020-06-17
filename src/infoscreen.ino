#include <Arduino.h>
#include <ESP8266WiFi.h>
#define MQTT_MAX_PACKET_SIZE 256
#include <WiFiClientSecure.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BME280.h>
#include "credentials.h"
#include "config.h"

const char *hostname = WIFI_HOSTNAME;
const char *ssid = WIFI_SSID;
const char *password =  WIFI_PASSWORD;
const char *mqttServer = MQTT_SERVER;
const int mqttPort = MQTT_PORT;
const char *mqttUser = MQTT_USERNAME;
const char *mqttPassword = MQTT_PASSWORD;
const char *mqttID = MQTT_ID;

unsigned long heartbeat_previousMillis = 0;
const long heartbeat_interval = HEARTBEAT_INTERVALL;

float temperature_cur;
float temperature_min;
float temperature_max;
float humidity;
float pressure;
float brightness;
String current_date;
String current_time;
String current_weekday;
float temperature_local;
float humidity_local;
float pressure_local;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_BME280 bme;

WiFiClientSecure espClient;
PubSubClient client(espClient);
 
void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  pinMode(LED_BUILTIN, OUTPUT);
  espClient.setInsecure();
  I2CAddressFinder();
  initializeDisplay();
  initializeBME280();
  reconnect();
}

void reconnect() {
  while (!client.connected()) {
    WiFi.mode(WIFI_STA);
	  WiFi.hostname(hostname);
    delay(100);
    Serial.println();
    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println();
    Serial.println("Connected to WiFi network");
    Serial.print("  SSID: ");
    Serial.print(ssid);
    Serial.print(" / Channel: ");
    Serial.println(WiFi.channel());
    Serial.print("  IP Address: ");
    Serial.print(WiFi.localIP());
    Serial.print(" / Subnet Mask: ");
    Serial.println(WiFi.subnetMask());
    Serial.print("  Gateway: ");
    Serial.print(WiFi.gatewayIP());
    Serial.print(" / DNS: ");
    Serial.print(WiFi.dnsIP());
    Serial.print(", ");
    Serial.println(WiFi.dnsIP(1));
    Serial.println("");

    // https://pubsubclient.knolleary.net/api.html
    client.setServer(mqttServer, mqttPort);
    client.setCallback(callback);
    Serial.println("Connecting to MQTT broker");
    Serial.print("  MQTT Server: ");
    Serial.println(mqttServer);
    Serial.print("  MQTT Port: ");
    Serial.println(mqttPort);
    Serial.print("  MQTT Username: ");
    Serial.println(mqttUser);
    Serial.print("  MQTT Identifier: ");
    Serial.println(mqttID);
    Serial.println("");

    while (!client.connected()) {
      if (client.connect(mqttID, mqttUser, mqttPassword)) {
        Serial.println("Connected to MQTT broker");
        Serial.println("Subscribe MQTT Topics");
        client.subscribe("home/outdoor/weather/temperature");
        client.subscribe("home/outdoor/weather/temperature/max");
        client.subscribe("home/outdoor/weather/temperature/min");
        client.subscribe("home/outdoor/weather/humidity");
        client.subscribe("home/outdoor/weather/pressure");
        client.subscribe("home/outdoor/weather/brightness");
        client.subscribe("home/datetime/currentdate");
        client.subscribe("home/datetime/currenttime");
        client.subscribe("home/datetime/currentweekday");
        Serial.println("");
        digitalWrite(LED_BUILTIN, HIGH); 
       } else {
        Serial.print("Connection to MQTT broker failed with state: ");
        Serial.println(client.state());
        char puffer[100];
        espClient.getLastSSLError(puffer,sizeof(puffer));
        Serial.print("TLS connection failed with state: ");
        Serial.println(puffer);
        Serial.println("");
        delay(4000);
       }
    }
  }
}

// Function to receive MQTT messages
void mqttloop() {
  if (!client.loop())
    client.connect(mqttID);
}

// Function to send MQTT messages
void mqttsend(const char *_topic, const char *_data) {
  client.publish(_topic, _data);
}

// Pointer to a message callback function called when a message arrives for a subscription created by this client.
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message topic: ");
  Serial.print(topic);
  Serial.print(" | Message Payload: ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println("");
  MqttToVar(topic, payload, length);  
}

void loop() {
  client.loop();
  reconnect();
  heartbeat();
  loopScenes();
  mqttloop();
}

void MqttToVar(char* topic, byte* payload, unsigned int length) {
  String mqttTopic = String(topic);
  String mqttPayload;
  for (unsigned int i = 0; i < length; i++) {
    mqttPayload += (char)payload[i];
  }
  if (mqttTopic == "home/outdoor/weather/temperature") {
    temperature_cur = mqttPayload.toFloat();
  } 
    else if (mqttTopic == "home/outdoor/weather/temperature/min"){
    temperature_min = mqttPayload.toFloat();
  } 
    else if (mqttTopic == "home/outdoor/weather/temperature/max"){
    temperature_max = mqttPayload.toFloat();
  } 
  else if (mqttTopic == "home/outdoor/weather/humidity"){
    humidity = mqttPayload.toFloat();
  } 
  else if (mqttTopic == "home/outdoor/weather/pressure"){
    pressure = mqttPayload.toFloat();
  } 
  else if (mqttTopic == "home/outdoor/weather/brightness"){
    brightness = mqttPayload.toFloat();
  }
  else if (mqttTopic == "home/datetime/currentdate"){
    current_date = mqttPayload;
  }
  else if (mqttTopic == "home/datetime/currenttime"){
    current_time = mqttPayload;
  }
  else if (mqttTopic == "home/datetime/currentweekday"){
    current_weekday = mqttPayload;
  }
}

int i=0;
unsigned long loopDisplay_previousMillis = 0;
const long loopDisplay_interval = LOOPDISPLAY_INTERVALL;

void loopScenes() {
  unsigned long loopDisplay_currentMillis = millis();
  if (loopDisplay_currentMillis - loopDisplay_previousMillis >= loopDisplay_interval) {
    loopDisplay_previousMillis = loopDisplay_currentMillis;
    
    int contrast;
    int contrast_high = 411;
    int contrast_low = 100;
    float brightness_thresshold = 40;
    if (brightness <= brightness_thresshold) {
      while (contrast != contrast_low) { 
        Serial.println("Brightness lt 40, set LCD contrast to 100");
        setContrast(contrast_low);
        contrast=contrast_low;
      }
    }else{
      while (contrast != contrast_high) { 
        Serial.println("Brightness gt 40, set LCD contrast to 411");
        setContrast(contrast_high);
        contrast=contrast_high;
      }
    }
    
    if (i==0) {
      sceneNetwork();
      i++;
    } else if (i==1) {
      sceneEnvIn();
      i++;
    } else if (i==2) {
      sceneEnvOut();
      i++;
    } else if (i==3) {
      sceneDateTime();
      i=1;
    }

    readsensor_bme280();
  }
}

void heartbeat() {
  unsigned long heartbeat_currentMillis = millis();
  if (heartbeat_currentMillis - heartbeat_previousMillis >= heartbeat_interval) {
    heartbeat_previousMillis = heartbeat_currentMillis;
    Serial.println("Send heartbeat signal to MQTT broker");
    Serial.println("");
    client.publish("home/outdoor/infoscreen/heartbeat", "on");
  }
}

void I2CAddressFinder() {
  Serial.println ();
  Serial.println ("I2C scanner. Scanning ...");
  byte count = 0;
  pinMode(13,OUTPUT); 
  digitalWrite(13,HIGH);
  Wire.begin();
  for (byte i = 1; i < 120; i++) {
    Wire.beginTransmission (i);
    if (Wire.endTransmission () == 0) {
      Serial.print ("Found address: ");
      Serial.print (i, DEC);
      Serial.print (" (0x");
      Serial.print (i, HEX);
      Serial.println (")");
      count++;
      delay (1); 
      } 
  } 
  Serial.print ("Found ");
  Serial.print (count, DEC);
  Serial.println (" device(s).");
  Serial.println ("");
}

void initializeDisplay() {
  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  Serial.print("Initialize SSD1306 display: ");
  if(!display.begin(SSD1306_SWITCHCAPVCC, I2C_ADDRESS_OLED)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }else{
    Serial.println("OK");
  }
  display.clearDisplay();
  display.display();

  display.drawPixel(0, 0, WHITE);
  display.drawPixel(127, 0, WHITE);
  display.drawPixel(0, 63, WHITE);
  display.drawPixel(127, 63, WHITE);

  display.drawBitmap(0, 0, logo_64x64, 64, 64, WHITE);

  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(75,10);
  display.print("Booting");
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(70,25);
  display.print("INFO");
  display.setCursor(45,45);
  display.print("SCREEN");

  display.display();
}

// for Adafruit_SSD1306.h library, contr values from 1 to 411
void setContrast(int contr){
    int prech;
    int brigh; 
    switch (contr){
      case 001 ... 255: prech= 0; brigh= contr; break;
      case 256 ... 411: prech=16; brigh= contr-156; break;
      default: prech= 16; brigh= 255; break;}
      
    display.ssd1306_command(SSD1306_SETPRECHARGE);      
    display.ssd1306_command(prech);                            
    display.ssd1306_command(SSD1306_SETCONTRAST);         
    display.ssd1306_command(brigh);                           
}

void sceneNetwork() {
  Serial.println ("Network informations");
  display.clearDisplay();
  
  display.setTextSize(1); 
  display.setCursor(0,0);
  display.print(ssid);
  display.print(" CH:");
  display.println(WiFi.channel());
  display.println(WiFi.localIP());
  display.println(hostname);
  display.print(mqttServer);
  display.print(" ");
  display.println(mqttPort);
  display.print("connection status:");
  display.println(client.connected());
    
  display.display();
}

void drawBitmap(int16_t x, int16_t y, uint8_t *bitmap, int16_t w, int16_t h, uint16_t color);

void sceneEnvIn() {
  display.clearDisplay();
  Serial.print ("Temperature (current): ");
  Serial.println (temperature_local);
  display.setTextSize(1); 
  display.setCursor(0,0);
  display.print("Temperature: ");
  display.setTextSize(2);
  display.setCursor(0,10);
  display.print(temperature_local);
  display.setTextSize(1);
  display.cp437(true);
  display.write(167);
  display.setTextSize(1);
  display.print("C");

  Serial.print ("Humidity: ");
  Serial.println (humidity_local);
  display.setTextSize(1);
  display.setCursor(0,35);
  display.print("Humidity: ");
  display.setTextSize(2);
  display.setCursor(0,45);
  display.print(humidity_local);
  display.setTextSize(1);
  display.print("%"); 

  display.setTextSize(2);
  display.setCursor(90,0);
  display.print("IN");

  display.drawBitmap(90, 30, logo_32x32, 32, 32, WHITE);

  display.display(); 
}

void sceneEnvOut() {
  display.clearDisplay();
  Serial.print ("Temperature (current): ");
  Serial.println (temperature_cur);
  display.setTextSize(1); 
  display.setCursor(0,0);
  display.print("Temperature: ");
  display.setTextSize(2);
  display.setCursor(0,10);
  display.print(temperature_cur);
  display.setTextSize(1);
  display.cp437(true);
  display.write(167);
  display.setTextSize(1);
  display.print("C");

  Serial.print ("Humidity: ");
  Serial.println (humidity);
  display.setTextSize(1);
  display.setCursor(0,35);
  display.print("Humidity: ");
  display.setTextSize(2);
  display.setCursor(0,45);
  display.print(humidity);
  display.setTextSize(1);
  display.print("%"); 

  display.setTextSize(2);
  display.setCursor(90,0);
  display.print("OUT");

  Serial.print ("Temperature (max): ");
  Serial.println (temperature_max);
  display.setTextSize(1);
  display.setCursor(90,20);
  display.print("Max: ");
  display.setTextSize(1);
  display.setCursor(90,30);
  display.print(temperature_max);

  Serial.print ("Temperature (min): ");
  Serial.println (temperature_min);
  display.setTextSize(1);
  display.setCursor(90,45);
  display.print("Min: ");
  display.setTextSize(1);
  display.setCursor(90,55);
  display.print(temperature_min);

  display.display(); 
}

void sceneDateTime() {
  display.clearDisplay();
  Serial.print ("Date: ");
  Serial.print (current_weekday);
  Serial.print (", ");
  Serial.println (current_date);
  display.setTextSize(1); 
  display.setCursor(0,0);
  display.print("   ");
  display.println(current_weekday);
  display.print("   ");
  display.print(current_date);

  Serial.print ("Time: ");
  Serial.println (current_time);
  display.setTextSize(2.5,5); 
  display.setCursor(17,25);
  display.print(current_time);
  display.print(" ");

  display.display(); 
}

void initializeBME280() {
  Serial.print("Initialize BME280 module: ");
  if (!bme.begin(I2C_ADDRESS_BME280)) {
    Serial.println(F("BME280 allocation failed"));
  } else {
    Serial.println("OK");
  }
}

void readsensor_bme280() {
  temperature_local = bme.readTemperature();
  Serial.print("Temperature: ");
  Serial.print(temperature_local);
  Serial.println(" *C");
  static char temperature_local_char[7];
  dtostrf(temperature_local, 1, 2, temperature_local_char);
  Serial.print("  MQTT publish home/indoor/infoscreen/temperature: ");
  Serial.println(temperature_local_char);
  client.publish("home/indoor/infoscreen/temperature", temperature_local_char, true);
  delay(100);

  humidity_local = bme.readHumidity();
  Serial.print("Humidity: ");
  Serial.print(humidity_local);
  Serial.println(" %");
  static char humidity_local_char[7];
  dtostrf(humidity_local, 1, 2, humidity_local_char);
  Serial.print("  MQTT publish home/indoor/infoscreen/humidity: ");
  Serial.println(humidity_local_char);
  client.publish("home/indoor/infoscreen/humidity", humidity_local_char, true);
  delay(100);

  pressure_local = (bme.readPressure() / 100.0F);
  Serial.print("Pressure: ");
  Serial.print(pressure_local);
  Serial.println(" hPa");
  static char pressure_local_char[7];
  dtostrf(pressure_local, 1, 2, pressure_local_char);
  Serial.print("  MQTT publish home/indoor/infoscreen/pressure: ");
  Serial.println(pressure_local_char);
  client.publish("home/indoor/infoscreen/pressure", pressure_local_char, true); // Pressure (hPa)
  delay(100);
}
