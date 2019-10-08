#include <WiFi.h> //https://github.com/espressif/arduino-esp32
#include <HTTPClient.h> //https://github.com/espressif/arduino-esp32
#include <AutoConnect.h> //https://github.com/Hieromon/AutoConnect

#define CORE1 0

AutoConnect Portal;
AutoConnectConfig Config;



const int pin_r = 4;
const int pin_g = 16;
const int pin_b = 17;
const int pin_btn1 = 5;  // :(
const int pin_btn2 = 18; // :|
const int pin_btn3 = 19; // :)


// Input pins for event code
const int dips_input[6] = {14, 27, 26, 25, 33, 32};

// Edit these
const char *url = "";
const char *ingest_api_key = "";
const char *hotspot_name = "EventBox";
const char *hotspot_password = "";


unsigned long last_data_push = 0;
unsigned long last_button_push = 0;
unsigned long last_eventCode_check = 0;
unsigned long last_light_blink = 0;

bool lightBlink = true;
bool buttonBlink = false;
bool setupDone = false;
bool buttonIsPushed = false;

int positivePushCounter = 0;
int neutralPushCounter = 0;
int negativePushCounter = 0;

int debounce_count = 500;
int debounce_counter = 0;

int negativeValue = 0;
int neutralValue = 0;
int positiveValue = 0;

String eventCode = "";

void setup() {
  Serial.begin(9600);
  delay(20);
  pinMode(pin_r, OUTPUT);
  pinMode(pin_g, OUTPUT);
  pinMode(pin_b, OUTPUT);
  pinMode(pin_btn1, INPUT);
  pinMode(pin_btn2, INPUT);
  pinMode(pin_btn3, INPUT);

  for (int i = 0; i < 6; i++)
  {
    pinMode(dips_input[i], INPUT);
  }

  Config.apid = hotspot_name;
  Config.psk = hotspot_password;
  Config.autoReconnect = true;
  Config.immediateStart = true;
  Portal.config(Config);

  xTaskCreate(loopTwo, "loopTwo", 8192, NULL, 1, NULL);
  Portal.begin();
  setupDone = true;
  last_data_push = millis();

}

void loop() {
  
  Portal.handleClient();
  
    String curr_eventCode = "";
  for (int i = 0; i < 6; i++)
  {
    int pin_i = digitalRead(dips_input[i]);
    curr_eventCode += String(pin_i);
  }

  if (last_data_push + 30000 < millis() || curr_eventCode != eventCode)
  {
    int send_negative = negativePushCounter;
    int send_nuteral = neutralPushCounter;
    int send_positive = positivePushCounter;

    if (WiFi.isConnected() == 1)
    {
      if (send_negative != 0 || send_nuteral != 0 || send_positive != 0)
      {
        HTTPClient http;
        http.begin(url);
        http.addHeader("Content-Type", "application/json");
        http.addHeader("x-api-key", ingest_api_key);

        String post = "{\"event_code\": ";
        post += eventCode;
        post += ", \"negative_count\": ";
        post += send_negative;
        post += ", \"neutral_count\": ";
        post += send_nuteral;
        post += ", \"positive_count\": ";
        post += send_positive;
        post += "\"}";

        Serial.print("SENDING: ");
        Serial.println(post);

        int response = http.POST(post);
        if (response >= 200 && response < 300)
        {
          Serial.println(response);
          Serial.println(http.getString());

          //negativePushCounter -= send_negative;
          //neutralPushCounter -= send_nuteral;
          //positivePushCounter -= send_positive;
        }
        else
        {
          Serial.print("Error on POST:");
          Serial.print(response);
          Serial.println(http.getString());
        }
        http.end();
      }
    }

    last_data_push = millis();
  }

  if (eventCode != curr_eventCode)
  {
    Serial.println("eventcode changed");
    Serial.println("resetting counters...");
    eventCode = curr_eventCode;
    positivePushCounter = 0;
    neutralPushCounter = 0;
    negativePushCounter = 0;
  }
  last_eventCode_check = millis();
}





void loopTwo(void *pvParameters) {
  while(true){
    diodeControl();

    if(setupDone){
      bool btn_negative = digitalRead(pin_btn1);
      bool btn_neutral = digitalRead(pin_btn2);
      bool btn_positive = digitalRead(pin_btn3);

      if(!buttonIsPushed){
        if(btn_negative){
          negativeValue = 1;
          buttonPush();
        }
        else if(btn_neutral){
          neutralValue = 1;
          buttonPush();
        }
        else if(btn_positive){
          positiveValue = 1;
          buttonPush();
        }
      }
      else if(!btn_negative && !btn_neutral && !btn_positive){
        buttonIsPushed = false;
        valueReset();
      }
      else if(debounce_counter == debounce_count){
        negativePushCounter += negativeValue;
        neutralPushCounter += neutralValue;
        positivePushCounter += positiveValue;
        debounce_counter++;
        valueReset();
        printPushes();
      }
      else{
        debounce_counter++;
      }
    }
  }
}

void valueReset(){
  negativeValue = 0;
  neutralValue = 0;
  positiveValue = 0;
}


void buttonPush(){
  buttonIsPushed = true;
  diodeBlink();
  buttonBlink = true;
  digitalWrite(pin_g, HIGH);
  debounce_counter = 0;
}

void diodeControl(){
  if (!lightBlink)
  {
    diodeLightReset();
    digitalWrite(pin_b, HIGH);
  }

  if (WiFi.isConnected() == 0 && !lightBlink)
  {
    diodeBlink();
    digitalWrite(pin_r, HIGH);
  }

  if (lightBlink && last_light_blink + 200 < millis())
    {
      if (!buttonBlink)
      {
        diodeLightReset();
      }
      else
      {
        lightBlink = false;
        buttonBlink = false;
        diodeLightReset();
        digitalWrite(pin_b, HIGH);
        last_light_blink = millis();
      }
      if (last_light_blink + 400 < millis())
      {
        lightBlink = false;
        last_light_blink = millis();
      }
    }
}

void diodeLightReset()
{
  digitalWrite(pin_r, LOW);
  digitalWrite(pin_g, LOW);
  digitalWrite(pin_b, LOW);
}

void diodeBlink()
{
  diodeLightReset();
  lightBlink = true;
  last_light_blink = millis();
}

void printPushes()
{
  Serial.print("postive: ");
  Serial.print(positivePushCounter);
  Serial.print(" neutral: ");
  Serial.print(neutralPushCounter);
  Serial.print(" negative: ");
  Serial.println(negativePushCounter);
}
