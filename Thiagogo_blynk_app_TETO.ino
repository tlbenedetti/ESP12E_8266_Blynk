/*
   1MB flash sizee
   Thiagogo esp8266-01
   gpio  0 - button
   gpio 2 - relay
   gpio 1 - green led - active low
   gpio 3 (RX) - interruptor externo
*/

#define SONOFF_BUTTON   0
#define SONOFF_RELAY    12
#define SONOFF_LED      13
#define INTERRUPTOR     3

#define BLYNK_PRINT Serial    // Comment this out to disable prints and save space
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

static bool BLYNK_ENABLED = true;

#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

#include <EEPROM.h>

#define EEPROM_SALT 12663
typedef struct {
  int   salt = EEPROM_SALT;
  char  blynkToken[33]  = "";
  char  blynkServer[33] = "blynk-cloud.com";
  char  blynkPort[6]    = "8442";
} WMSettings;

WMSettings settings;

#include <ArduinoOTA.h>


//for LED status
#include <Ticker.h>
Ticker ticker;


const int CMD_WAIT = 0;
const int CMD_BUTTON_CHANGE = 1;

int cmd = CMD_WAIT;
int relayState = HIGH;

//inverted button state
int buttonState = LOW;

int ultimoestadobotao = HIGH; //interruptor externo

static long startPress = 0;

int estadobotao;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50; //debounce botão

void tick()
{
  //toggle state
  int state = digitalRead(SONOFF_LED);  // get the current state of GPIO1 pin
  digitalWrite(SONOFF_LED, !state);     // set pin to the opposite state
}

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  //Serial.println("Entered config mode");
  //Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  //Serial.println(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);
}



void setState(int s) {
  digitalWrite(SONOFF_RELAY, s);
  digitalWrite(SONOFF_LED, s % 2); // led is active low
  Blynk.virtualWrite(6, s*255);
}

void turnOn() {
  relayState = LOW;
  setState(relayState);
}

void turnOff() {
  relayState = HIGH;
  setState(relayState);
}

void toggleState() {
  cmd = CMD_BUTTON_CHANGE;
}

//flag for saving data
bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  //Serial.println("Should save config");
  shouldSaveConfig = true;
}


void toggle() {
  //Serial.println("toggle state");
  //Serial.println(relayState);
  relayState = relayState == HIGH ? LOW : HIGH;
  setState(relayState);
}

void restart() {
  ESP.reset();
  delay(1000);
}

void apaga() {
  //reset settings to defaults
  
    WMSettings defaults;
    settings = defaults;
    EEPROM.begin(512);
    EEPROM.put(0, settings);
    EEPROM.end();
  
  //reset wifi credentials
  WiFi.disconnect();
  delay(1000);
  ESP.restart();
  delay(1000);
}


//off - button
BLYNK_WRITE(0) {
  int a = param.asInt();
  if (a != 0) {
    turnOff();
  }
}

//on - button
BLYNK_WRITE(1) {
  int a = param.asInt();
  if (a != 0) {
    turnOn();
  }
}

//toggle - button
BLYNK_WRITE(2) {
  int a = param.asInt();
  if (a != 0) {
    toggle();
  }
}

//restart - button
BLYNK_WRITE(3) {
  int a = param.asInt();
  if (a != 0) {
    restart();
  }
}

//restart - button
BLYNK_WRITE(4) {
  int a = param.asInt();
  if (a != 0) {
    apaga();
  }
}

//status - display
BLYNK_READ(5) {
  Blynk.virtualWrite(5, relayState);
}

void setup()
{
  //Serial.begin(115200);

  //set led pin as output
  pinMode(SONOFF_LED, OUTPUT);

  pinMode(SONOFF_RELAY, OUTPUT);

  // start ticker with 0.5 because we start in AP mode and try to connect
  ticker.attach(0.6, tick);

   
  //setup relay lampada on para instalaçao no teto
  pinMode(SONOFF_RELAY, OUTPUT);

  turnOn();



  const char *hostname = "THIAGOGO";

  WiFiManager wifiManager;
  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  //timeout - this will quit WiFiManager if it's not configured in 3 minutes, causing a restart
  wifiManager.setConfigPortalTimeout(180);

  //custom params
  EEPROM.begin(512);
  EEPROM.get(0, settings);
  EEPROM.end();

  if (settings.salt != EEPROM_SALT) {
    //Serial.println("Invalid settings in EEPROM, trying with defaults");
    WMSettings defaults;
    settings = defaults;
  }

  //Serial.println(settings.blynkToken);
  //Serial.println(settings.blynkServer);
  //Serial.println(settings.blynkPort);

  WiFiManagerParameter custom_blynk_text("Blynk config. <br/> No token to disable.");
  wifiManager.addParameter(&custom_blynk_text);

  WiFiManagerParameter custom_blynk_token("blynk-token", "blynk token", settings.blynkToken, 33);
  wifiManager.addParameter(&custom_blynk_token);

  WiFiManagerParameter custom_blynk_server("blynk-server", "blynk server", settings.blynkServer, 33);
  wifiManager.addParameter(&custom_blynk_server);

  WiFiManagerParameter custom_blynk_port("blynk-port", "blynk port", settings.blynkPort, 6);
  wifiManager.addParameter(&custom_blynk_port);

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  if (!wifiManager.autoConnect(hostname)) {
    //Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

  //Serial.println(custom_blynk_token.getValue());
  //save the custom parameters to FS
  if (shouldSaveConfig) {
    //Serial.println("Saving config");

    strcpy(settings.blynkToken, custom_blynk_token.getValue());
    strcpy(settings.blynkServer, custom_blynk_server.getValue());
    strcpy(settings.blynkPort, custom_blynk_port.getValue());

    //Serial.println(settings.blynkToken);
    //Serial.println(settings.blynkServer);
    //Serial.println(settings.blynkPort);

    EEPROM.begin(512);
    EEPROM.put(0, settings);
    EEPROM.end();
  }

  //config blynk
  if (strlen(settings.blynkToken) == 0) {
    BLYNK_ENABLED = false;
  }
  if (BLYNK_ENABLED) {
    Blynk.config(settings.blynkToken, settings.blynkServer, atoi(settings.blynkPort));
  }

  //OTA
  ArduinoOTA.onStart([]() {
    //Serial.println("Start OTA");
  });
  ArduinoOTA.onEnd([]() {
    //Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    //Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  /*ArduinoOTA.onError([](ota_error_t error) {
    //Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) //Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) //Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) //Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) //Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) //Serial.println("End Failed");
  });*/
  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.begin();

  //if you get here you have connected to the WiFi
  //Serial.println("connected...yeey :)");
  ticker.detach();

  //setup button
  pinMode(SONOFF_BUTTON, INPUT_PULLUP);
  attachInterrupt(SONOFF_BUTTON, toggleState, CHANGE);
  pinMode(INTERRUPTOR, INPUT_PULLUP);    // Define o pino do interruptor como entrada
  attachInterrupt(INTERRUPTOR, toggleState, CHANGE);
  
    //setup relay

    
  //Serial.println("done setup");
}


void loop()
{

  //ota loop
  ArduinoOTA.handle();
  //blynk connect and run loop
  if (BLYNK_ENABLED) {
    Blynk.run();
  }
 
  //delay(200);
  //Serial.println(digitalRead(SONOFF_BUTTON));
  switch (cmd) {
    case CMD_WAIT:
      break;
    case CMD_BUTTON_CHANGE:
      int currentState = digitalRead(SONOFF_BUTTON);
      if (currentState != buttonState) {
        if (buttonState == LOW && currentState == HIGH) {
          long duration = millis() - startPress;
          if (duration < 3000) {
            //Serial.println("short press - toggle relay");
            toggle();
          }  else if (duration < 10000) {
            //Serial.println("long press - reset settings");
            apaga();
          }
        } else if (buttonState == HIGH && currentState == LOW) {
          startPress = millis();
        }
        buttonState = currentState;
      }
      break;
  }



//botão externo interruptor de luz normal

int sensorval = digitalRead(INTERRUPTOR);

   if(sensorval != ultimoestadobotao){
    lastDebounceTime = millis();
   }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (sensorval != estadobotao) {
      estadobotao = sensorval;
      //para usar botão tactil push button, descomentar o if
     // if (estadobotao == LOW) {
         toggle();
      //}
    }
  }
ultimoestadobotao = sensorval;

}



