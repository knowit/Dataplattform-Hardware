#include <WiFi.h>
#include <HTTPClient.h>
#include <AutoConnect.h>

#define CORE1 0

AutoConnect Portal;
AutoConnectConfig Config;

const int pin_r = 4;
const int pin_g = 16;
const int pin_b = 17;
const int pin_btn1 = 5; // :(
const int pin_btn2 = 18; // :|
const int pin_btn3 = 19; // :)

// Edit these
const char* ssid = "";
const char* password = "";
const char* url = "https://[...].execute-api.eu-central-1.amazonaws.com/prod/dataplattform_ingest/EventRatingType";
const char* ingest_api_key = "";

// Input pins for event code
const int dips_input[6] = {14, 27, 26, 25, 33, 32};

unsigned long last_wifi_check = 0;
unsigned long last_light_flip = 0;
bool startUp = true;

void setup() {
    Serial.begin(115200);
    delay(20);

    pinMode(pin_r, OUTPUT);
    pinMode(pin_g, OUTPUT);
    pinMode(pin_b, OUTPUT);
    pinMode(pin_btn1, INPUT);
    pinMode(pin_btn2, INPUT);
    pinMode(pin_btn3, INPUT);

    for (int i = 0; i < 6; i++) {
        pinMode(dips_input[i], INPUT);
    }

    Config.immediateStart = true;
    Portal.config(Config);

    xTaskCreate( loop2, "loop2", 4096, NULL, 1, NULL );
    
    Serial.println("Startup");
    connect_to_wifi();
    last_wifi_check = millis();
}

void connect_to_wifi() {
    Serial.print("URL: ");
    Serial.println(url);
    Serial.print("Connecting to ");
    Serial.println(ssid);
    Serial.print("Password: ");
    Serial.println(password);

    Portal.begin();

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    startUp = false;
    diodeLightShuffle();
}

void diodeLightReset() {
  digitalWrite(pin_r, LOW);
  digitalWrite(pin_g, LOW);
  digitalWrite(pin_b, LOW);
}

void diodeLightLength(int selectedPin, int selectedDelay){
  digitalWrite(selectedPin, HIGH);
  delay(selectedDelay);
  digitalWrite(selectedPin, LOW);
}

void diodeLightShuffle(){
  diodeLightReset();
  diodeLightLength(pin_r,250);
  diodeLightLength(pin_g,250);
  diodeLightLength(pin_b,250);
  diodeLightLength(pin_r,250);
  diodeLightLength(pin_g,250);
  diodeLightLength(pin_b,250);
  diodeLightLength(pin_r,250);
  diodeLightLength(pin_g,250);
  diodeLightLength(pin_b,250);
  diodeLightLength(pin_r,250);
  diodeLightLength(pin_g,250);
  diodeLightLength(pin_b,250);
}

void loop() {
    Portal.handleClient();
    int btn1 = digitalRead(pin_btn1);
    int btn2 = digitalRead(pin_btn2);
    int btn3 = digitalRead(pin_btn3);
    if (btn1 | btn2 | btn3) {
        if (WiFi.status() != WL_CONNECTED) {
            connect_to_wifi();
        }
        diodeLightReset();
        digitalWrite(pin_b, HIGH);
        HTTPClient http;
        http.begin(url);
        http.addHeader("Content-Type", "application/json");
        http.addHeader("x-api-key", ingest_api_key);

        String post = "{\"button\": ";
        if (btn1) {
            post += "-1, ";
        } else if (btn2) {
            post += "0, ";
        } else if (btn3) {
            post += "1, ";
        }
        post += "\"event_code\": \"";
        String event_code = "";
        for (int i = 0; i < 6; i++) {
            int pin_i = digitalRead(dips_input[i]);
            event_code += String(pin_i);
        }
        post += event_code;
        post += "\"}";
        Serial.println(post);
        int response = http.POST(post);
        delay(500);
        diodeLightReset();
        if (response >= 200 && response < 300) {
            digitalWrite(pin_g, HIGH);
            Serial.println(response);
            Serial.println(http.getString());
        } else {
            digitalWrite(pin_r, HIGH);
            Serial.print("Error on POST: ");
            Serial.println(response);
            Serial.println(http.getString());
            delay(500);
        }
        http.end();
        delay(500);
        diodeLightReset();
    }

    // Check WiFi every 30 secs
    unsigned long current_millis = millis();
    if (last_wifi_check + 30000 < current_millis) {
        if (WiFi.status() != WL_CONNECTED) {
            diodeLightLength(pin_b,250);
            delay(250);
            connect_to_wifi();
        }
        last_wifi_check = current_millis;
    }
    
}

void loop2(void *pvParameters) {
  while (1) {
     // Check light switch every 0.30 secs
    unsigned long current_millis = millis();
    if (startUp){
      if (last_light_flip + 300 < current_millis) {
        if (digitalRead(pin_b) == HIGH) {
            digitalWrite(pin_b, LOW);
        }else{
            digitalWrite(pin_b, HIGH);
        }
        last_light_flip = current_millis;
      }
    }else{
      if(digitalRead(pin_b) == LOW){
        digitalWrite(pin_b, HIGH);
      }
    }
  }
}
