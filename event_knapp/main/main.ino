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
const char* url = "";
const char* ingest_api_key = "";

// Input pins for event code
const int dips_input[6] = {14, 27, 26, 25, 33, 32};

// Different counters to time next event
unsigned long last_wifi_check = 0;
unsigned long last_light_flip = 0;
unsigned long last_data_push = 0;

// Variables to use when using different events
bool startUp = true;
int messages[200];
int messagePosition = 0;

bool btn1_state = false;
bool btn2_state = false;
bool btn3_state = false;

int buttonPushCounter = 0;
int positivePushCounter = 0;
int neutralPushCounter = 0;
int negativePushCounter = 0;

String eventCode = "";
unsigned long last_eventCode_check = 0;


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

  // Edit these
  Config.apid = "EventBox";
  Config.psk = "objectnet";

  Config.immediateStart = true;
  Portal.config(Config);

  xTaskCreate( newloop, "newloop", 8192, NULL, 1, NULL );

  Serial.println("Startup");
  connect_to_wifi();
  last_wifi_check = millis();
  last_data_push = millis();
  last_eventCode_check = millis();
}

void connect_to_wifi() {
  Serial.print("URL: ");
  Serial.println(url);

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

void diodeLightLength(int selectedPin, int selectedDelay) {
  digitalWrite(selectedPin, HIGH);
  delay(selectedDelay);
  digitalWrite(selectedPin, LOW);
}

void diodeLightShuffle() {
  diodeLightReset();
  diodeLightLength(pin_r, 250);
  diodeLightLength(pin_g, 250);
  diodeLightLength(pin_b, 250);
  diodeLightLength(pin_r, 250);
  diodeLightLength(pin_g, 250);
  diodeLightLength(pin_b, 250);
  diodeLightLength(pin_r, 250);
  diodeLightLength(pin_g, 250);
  diodeLightLength(pin_b, 250);
  diodeLightLength(pin_r, 250);
  diodeLightLength(pin_g, 250);
  diodeLightLength(pin_b, 250);
}

void loop() {
  Portal.handleClient();
  bool btn1 = digitalRead(pin_btn1);
  bool btn2 = digitalRead(pin_btn2);
  bool btn3 = digitalRead(pin_btn3);
  if (btn1 | btn2 | btn3) {
    if (WiFi.status() != WL_CONNECTED) {
      connect_to_wifi();
    }
    diodeLightReset();
    
    digitalWrite(pin_b, HIGH);
    
    

    if (btn1) {
      if (btn1_state != btn1) {
        Serial.println(":(");
        negativePushCounter++;
        buttonPushCounter++;
        printPushes();

        messages[messagePosition] = -1;
        btn1_state = btn1;
        messagePosition += 1;
      }
    } else if (btn2) {

      
      if (btn2_state != btn2) {
        Serial.println(":|");
        neutralPushCounter++;
        buttonPushCounter++;
        printPushes();

        messages[messagePosition] = 0;
        btn2_state = btn2;
        messagePosition += 1;
      }
    } else if (btn3) {
      

      if (btn3_state != btn3) {
        Serial.println(":)");
        positivePushCounter++;
        buttonPushCounter++;
        printPushes();

    
        messages[messagePosition] = 1;
        btn3_state = btn3;
        messagePosition += 1;
      }
    }

    diodeLightLength(pin_g, 100);

  } else {
    btn1_state = btn1;
    btn2_state = btn2;
    btn3_state = btn3;
  }

  
  // Check WiFi every 30 secs
  unsigned long current_millis = millis();
  if (last_wifi_check + 30000 < current_millis) {
    if (WiFi.status() != WL_CONNECTED) {
      diodeLightLength(pin_b, 250);
      delay(250);
      connect_to_wifi();
    }
    last_wifi_check = current_millis;
  }

  // Check eventcode every second
  if (last_eventCode_check + 1000 < current_millis) {
    String curr_eventCode = "";
    for (int i = 0; i < 6; i++) {
      int pin_i = digitalRead(dips_input[i]);
      curr_eventCode += String(pin_i);
      }
      if (eventCode != curr_eventCode) {
        Serial.println("eventcode changed");
        Serial.println("resetting counters...");
        eventCode = curr_eventCode;
        positivePushCounter = 0;
        neutralPushCounter = 0;
        negativePushCounter = 0;
        buttonPushCounter = 0;

      }
      last_eventCode_check = current_millis;   
  }

}


void printPushes() {
  Serial.print("number of button pushes: ");
  Serial.println(buttonPushCounter);
  Serial.print("postive: ");
  Serial.print(positivePushCounter);
  Serial.print(" neutral: ");
  Serial.print(neutralPushCounter);
  Serial.print(" negative: ");
  Serial.println(negativePushCounter);
}


void newloop(void *pvParameters) {
  int internalNegativePush = 0;
  int internalPositivePush = 0;
  int internalNeutralPush = 0;
  String internalEventCode = "";
  
  for(;;) {
    unsigned long current_millis = millis();
    if (startUp) {
      if (last_light_flip + 300 < current_millis) {
        if (digitalRead(pin_b) == HIGH) {
          digitalWrite(pin_b, LOW);
        } else {
          digitalWrite(pin_b, HIGH);
        }
        last_light_flip = current_millis;
      }
    } else {
      if (digitalRead(pin_b) == LOW) {
        digitalWrite(pin_b, HIGH);
      }

      //Send to aws every 30 sec
      if (last_data_push + 30000 < current_millis) {
  
        String globalEventCode = eventCode;
        //reset counters if internalEventCode has been changed
        if (internalEventCode != globalEventCode) {
          internalNegativePush = 0;
          internalPositivePush = 0;
          internalNeutralPush = 0;
  
        }
  
        int globalNeg = negativePushCounter;
        int globalPos = positivePushCounter;
        int globalNeu = neutralPushCounter;
  
        int changeNeg = globalNeg - internalNegativePush;
        int changePos = globalPos - internalPositivePush;
        int changeNeu = globalNeu - internalNeutralPush;
  
        if (changeNeg!=0 || changePos!=0 || changeNeu!=0) {
          Serial.println("change has occured");
  
          HTTPClient http;
          http.begin(url);
          http.addHeader("Content-Type", "application/json");
          http.addHeader("x-api-key", ingest_api_key);
          
          String post = "{\"positive_count\": ";
          post += changePos;
          post += ", \"negative_count\": ";
          post += changeNeg;
          post += ", \"neutral_count\": ";
          post += changeNeu;
          post += ", \"event_code\": \"";
          post += globalEventCode;
          post += "\"}";
  
          Serial.print("SENDING: ");
          Serial.println(post);
          
          int response = http.POST(post);
          if (response >= 200 && response < 300) {
            
            Serial.println(response);
            Serial.println(http.getString());
  
  
            //oppdaterer bare disse hvis vi har fått positiv respons fra AWS          
            internalNegativePush = globalNeg;
            internalPositivePush = globalPos;
            internalNeutralPush = globalNeu;
            
            } else {
            
            Serial.print("Error on POST: ");
            Serial.println(response);
            Serial.println(http.getString());
            }
          
          http.end();
  
          
          internalEventCode = eventCode;
  
        }
        
        last_data_push = current_millis;
      }
    }
  }
}
