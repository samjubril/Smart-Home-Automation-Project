#ifdef ENABLE_DEBUG
       #define DEBUG_ESP_PORT Serial
       #define NODEBUG_WEBSOCKETS
       #define NDEBUG
#endif 

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "SinricPro.h"
#include "SinricProSwitch.h"

#include <map>

#define WIFI_SSID         "SammNET"    
#define WIFI_PASS         "*******"
#define APP_KEY           "00b4f793-c3ed-4808-9cf2-121bbdb1aec5"      
#define APP_SECRET        "6815c7cc-5bba-4a82-b6cd-8d73bd1e9751-3a23e488-ad6d-4ab5-937e-32244d097bad"   
//Enter the device IDs here
#define device_ID_1   "6292b5f1c67e23532c8ec76e"   // Light Bulb
#define device_ID_2   "62a6f74efce0b9e02e6eaf54"   // Fan
#define device_ID_3   "62f03b73db2700373414caa4"   // Motion Sensor (Image Detector)


// define the GPIO connected with Relays and switches
#define RelayPin1 5  //D1
#define RelayPin2 14 //D5

#define SwitchPin1 9  // SD2
#define SwitchPin2 0   // D3 
#define SwitchPin3 13  // D7 Motion Sensor

#define wifiLed   16   //D0
#define BAUD_RATE   9600
#define DEBOUNCE_TIME 250

typedef struct {      // struct for the std::map below
  int relayPIN;
  int buttonSwitchPIN;
  
} deviceConfig_t;


std::map<String, deviceConfig_t> devices = {
    //{deviceId, {relayPIN,  buttonSwitchPIN}}
    {device_ID_1, {  RelayPin1, SwitchPin1 }},
    {device_ID_2, {  RelayPin2, SwitchPin2 }},      
};

typedef struct {      // struct for the std::map below
  String deviceId;
  bool lastbuttonSwitchState;
  unsigned long lastbuttonSwitchChange;
} buttonSwitchConfig_t;

std::map<int, buttonSwitchConfig_t> buttonSwitches;   
                                                  
void setupRelays() { 
  for (auto &device : devices) {           // for each device (relay, flipSwitch combination)
    int relayPIN = device.second.relayPIN; // get the relay pin
    pinMode(relayPIN, OUTPUT);             // set relay pin to OUTPUT
    digitalWrite(relayPIN, HIGH);
  }
}

void setupbuttonSwitches() {
  for (auto &device : devices)  {                     // for each device (relay / buttonSwitch combination)
    buttonSwitchConfig_t buttonSwitchConfig;              // create a new buttonSwitch configuration

    buttonSwitchConfig.deviceId = device.first;         // set the deviceId
    buttonSwitchConfig.lastbuttonSwitchChange = 0;        // set debounce time
    buttonSwitchConfig.lastbuttonSwitchState = true;     // set lastbuttonSwitchState to false (LOW)--

    int buttonSwitchPIN = device.second.buttonSwitchPIN;  // get the buttonSwitchPIN

    buttonSwitches[buttonSwitchPIN] = buttonSwitchConfig;   // save the buttonSwitch config to buttonSwitches map
    pinMode(buttonSwitchPIN, INPUT_PULLUP);                   // set the buttonSwitch pin to INPUT
  }
}

bool myPowerState = false;
unsigned long lastBtnPress = 0;

bool onPowerState(String deviceId, bool &state)
{
  Serial.printf("%s: %s\r\n", deviceId.c_str(), state ? "on" : "off");
  int relayPIN = devices[deviceId].relayPIN; // get the relay pin for corresponding device
  digitalWrite(relayPIN, !state);             // set the new relay state
  return true;
}

void handlebuttonSwitches() {
  unsigned long actualMillis = millis();                                  // get actual millis
  for (auto &buttonSwitch : buttonSwitches) {                // for each buttonSwitch in buttonSwitches map
    unsigned long lastbuttonSwitchChange = buttonSwitch.second.lastbuttonSwitchChange;  // get the timestamp when buttonSwitch was pressed last time (used to debounce / limit events)

    if (actualMillis - lastbuttonSwitchChange > DEBOUNCE_TIME) {                    // if time is > debounce time...

      int buttonSwitchPIN = buttonSwitch.first;                                       // get the buttonSwitch pin from configuration
      bool lastbuttonSwitchState = buttonSwitch.second.lastbuttonSwitchState;           // get the lastbuttonSwitchState
      bool buttonSwitchState = digitalRead(buttonSwitchPIN);                          // read the current buttonSwitch state
      if (buttonSwitchState != lastbuttonSwitchState) {                               // if the buttonSwitchState has changed...
#ifdef TACTILE_BUTTON
        if (buttonSwitchState) {                                                    // if the tactile button is pressed 
#endif      
          buttonSwitch.second.lastbuttonSwitchChange = actualMillis;                  // update lastbuttonSwitchChange time
          String deviceId = buttonSwitch.second.deviceId;                           // get the deviceId from config
          int relayPIN = devices[deviceId].relayPIN;                              // get the relayPIN from config
          bool newRelayState = !digitalRead(relayPIN);                            // set the new relay State
          digitalWrite(relayPIN, newRelayState);                                  // set the trelay to the new state

          SinricProSwitch &mySwitch = SinricPro[deviceId];                        // get Switch device from SinricPro
          mySwitch.sendPowerStateEvent(!newRelayState);                            // send the event
#ifdef TACTILE_BUTTON
        }
#endif      
        buttonSwitch.second.lastbuttonSwitchState = buttonSwitchState;                  // update lastbuttonSwitchState
      }
    }
  }
}

void setupWiFi()
{
  Serial.printf("\r\n[Wifi]: Connecting");
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.printf(".");
    delay(250);
  }
  digitalWrite(wifiLed, LOW);
  Serial.printf("connected!\r\n[WiFi]: IP-Address is %s\r\n", WiFi.localIP().toString().c_str());
}

// setup function for SinricPro
void setupSinricPro() 

{
  for (auto &device : devices)
  {
    const char *deviceId = device.first.c_str();
    SinricProSwitch &mySwitch = SinricPro[deviceId];
    mySwitch.onPowerState(onPowerState);
  }

  SinricPro.begin(APP_KEY, APP_SECRET);
  SinricPro.restoreDeviceStates(true);
}

int MotionSensor = D7;  // Pin 13
int Buzzer = D8;        // Pin 15


// main setup function
void setup()
{
  
  pinMode(SwitchPin1, INPUT);
  pinMode(SwitchPin2, INPUT); 
  pinMode(SwitchPin3, INPUT);  
    
    Serial.begin(BAUD_RATE); Serial.printf("\r\n\r\n");

  pinMode(wifiLed, OUTPUT);
  digitalWrite(wifiLed, HIGH);

  pinMode(MotionSensor, INPUT);  // Motion Sensor output pin connected
  pinMode(Buzzer, OUTPUT);       // Buzzer

  setupRelays();
  setupbuttonSwitches();
  setupWiFi();
  setupSinricPro();
}

void loop()
{ 
  SinricPro.handle();
  handlebuttonSwitches();

  
  int value = digitalRead(MotionSensor); // Motion Sensor output pin connected
  Serial.println(""); //see the value in serial monitor in Arduino IDE
  
  if (value == 0)
  {
    Serial.print("OBJECT IS PRESENT");
   digitalWrite(Buzzer, HIGH); // BUZZER ON; 
  }
  else
  {
   Serial.print("EMPTY");
   digitalWrite(Buzzer, LOW); //BUZZER OFF
  }
  delay(500);  
}
