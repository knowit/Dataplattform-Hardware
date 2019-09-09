#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <AutoConnect.h>

#include <OneWire.h>
#include <DallasTemperature.h>

AutoConnect Portal;
AutoConnectConfig Config;

#define AWS_LAMBDA ""
#define AWS_API_KEY ""
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// Data wire is plugged into pin 2 on the Arduino
#define ONE_WIRE_BUS 27

// Setup a oneWire instance to communicate with any OneWire devices
// (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);


// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

bool isIdentifying = false;

String macAdr = "";

const int MotionSensorSignal = 33;  // Digital port for the motion sensor
const int LightSensorSignal = 32;  // Pin for Analog Output - AO

float lightSensorValue = 0.0;
float tempSensorValue = 0.0;
int motionSensorValue = 0;

// Led Lights
int ledRedPin = 16;
int ledGreenPin = 26;

///////////////////////////////////////////

void sensorLoop() {
//MOTION
 motionSensorValue = digitalRead(MotionSensorSignal);
 Serial.print("This is the value from the motion sensor: ");
 Serial.println(motionSensorValue);

//TEMP
 // call sensors.requestTemperatures() to issue a global temperature
 // request to all devices on the bus
 sensors.requestTemperatures(); // Send the command to get temperatures

 Serial.print("Temperature is: ");
 tempSensorValue = sensors.getTempCByIndex(0);
 Serial.println(tempSensorValue);
 // You can have more than one IC on the same bus.
 // 0 refers to the first IC on the wire

//LIGHT
 lightSensorValue = analogRead(LightSensorSignal);  //Read the analog value
 Serial.print("Light is: ");
 Serial.println(lightSensorValue);  //Print the analog value
}

void blinkLED(int pin){
  digitalWrite(pin, HIGH);
  delay(500);
  digitalWrite(pin, LOW);
}

void connectToWifi() {
 Portal.begin();
 //WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
 Serial.print("Connecting to WIFI");
 
 while (WiFi.status() != WL_CONNECTED) {
   Serial.print(".");
   blinkLED(ledRedPin);
 }
 
 Serial.println();
 Serial.print("Connected with IP: ");
 Serial.println(WiFi.localIP());
}

void setupLEDs() {
  pinMode(ledRedPin, OUTPUT);
  pinMode(ledGreenPin, OUTPUT);
}

///////////////////////////////////////////

void setup() {
 Config.immediateStart = true;
 Portal.config(Config);
 Serial.begin(9600);
 setupLEDs();
 connectToWifi();

 macAdr = WiFi.macAddress();

 pinMode(MotionSensorSignal, INPUT); // declare digital pin 2 as input, this is where you connect the S output from your sensor, this can be any digital pin
 sensors.begin();
}

void loop() {
 Portal.handleClient();
 sensorLoop();

 DynamicJsonBuffer jsonBuffer;

 if(WiFi.status()== WL_CONNECTED){

   JsonObject& temperatureObject = jsonBuffer.createObject();
   temperatureObject["light"] = lightSensorValue;
   temperatureObject["motion"] = motionSensorValue;
   temperatureObject["temperature"] = tempSensorValue;
   
   WiFiClientSecure client;
 
   char JSONmessageBuffer[300];
   temperatureObject.prettyPrintTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
   Serial.println(JSONmessageBuffer);
 
   HTTPClient http;    //Declare object of class HTTPClient
 
   http.begin(AWS_LAMBDA);      //Specify request destination
   http.addHeader("Content-Type", "application/json");  //Specify content-type header
   http.addHeader("x-api-key", AWS_API_KEY);
 
   int httpCode = http.POST(JSONmessageBuffer);   //Send the request
   String payload = http.getString();                                        //Get the response payload
 
   Serial.println(httpCode);   //Print HTTP return code
   Serial.println(payload);    //Print request response payload
 
   http.end();  //Close connection
   blinkLED(ledGreenPin);
 }else{
  Serial.println("Error in WiFi connection");
  blinkLED(ledRedPin);
 }
 //Run every 5 min
 delay(300000);
}
