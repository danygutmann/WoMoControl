#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <EEPROM.h>

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

extern "C" {
#include "user_interface.h"
}
extern "C" {
#include "gpio.h"
}

const char* ssid     = "K2-NET";
const char* password = "Dikt81mp!";

byte DimSteps = 5;
byte PortsIn[] = {0,4,0,2};
byte PortsInState[] = {1,1,1,1};
long PortsInStateChange[] = {0,0,0,0};

byte PortsOut[] = {0,12,13,15};
byte StateOut[] = {0,0,0,0};
long IsDimming[] = {0,0,0,0};
String NextDimAction[] = {"null","null","null","null"}; 


void SetPort(byte Port, byte value){
  Serialout("SetPort " + String(Port) + " to " + value);
  analogWrite(PortsOut[Port], (1023*value)/100);
}

void Serialout(String Message) {
  Serial.println(Message);
}

void checkPin(byte Port){ 
  int inputRead = digitalRead(PortsIn[Port]);

  if (inputRead != PortsInState[Port]){
    // state changed
    PortsInState[Port] = inputRead;
    PortsInStateChange[Port] = millis();
    
    if (inputRead == 0){
      // warten wenn an
      delay(400);
      // wenn dann aus,
      if (digitalRead(PortsIn[Port]) == 1){
        // war es ein Pulse !!
        if (millis() - IsDimming[Port] > 1000) TogglePort(Port);
        
        }
    }

  }else{
      if (inputRead == 0){
        if ((millis() - PortsInStateChange[Port] > 600)) Dim(Port); 
      }
  }
      
 }

void Dim(byte Port){
  
  if (NextDimAction[Port] == "null"){
    if (StateOut[Port] > 0){
      Serialout("first dim for " + String(Port) + " (down)");
      NextDimAction[Port] = "down";
    }else{
      Serialout("first dim for " + String(Port) + " (up)");
      NextDimAction[Port] = "up";
    } 
  }

  IsDimming[Port] = millis();
  if (NextDimAction[Port] == "up"){
      Serialout("Dim up " + String(Port));
      if (StateOut[Port] < 10){
      StateOut[Port] = StateOut[Port] + 1;
      } else{
        StateOut[Port] = StateOut[Port] + DimSteps;
        }
      if (StateOut[Port] > 100 ) {
        StateOut[Port] = 100;
        NextDimAction[Port] = "down";
        }
        
    }else{
      Serialout("Dim down " + String(Port));
      if (StateOut[Port] < 10){
      StateOut[Port] = StateOut[Port] - 1;
      }else{
        StateOut[Port] = StateOut[Port] - DimSteps;
        }
      if (StateOut[Port] < 0 ) {
        StateOut[Port] = 0;
         NextDimAction[Port] = "up";
        }
      if (StateOut[Port] > 200 ) {
        StateOut[Port] = 0;
         NextDimAction[Port] = "up";
        }
    }  
   SetPort(Port, StateOut[Port]);
   delay(200);
}

void Dim(byte Port, String dir)
{
  if (dir == "heller"){
    StateOut[Port] = StateOut[Port] + DimSteps;
  }else{
    StateOut[Port] = StateOut[Port] - DimSteps;
  }
  SetPort(Port, StateOut[Port]);
}
void DimUp(byte Port){
  Serialout("DimUp " + String(Port));
  while(StateOut[Port] < 100) {
    StateOut[Port] = StateOut[Port] + DimSteps;
    if (StateOut[Port] > 100 ) StateOut[Port] = 100;
    SetPort(Port, StateOut[Port]);
    delay(20);
  }
}

void DimDown(byte Port){
  Serialout("DimDown " + String(Port));
  while(StateOut[Port] > 0) {
    
    if (StateOut[Port] < DimSteps ) {
      StateOut[Port] = 0;  
    }else{    
    StateOut[Port] = StateOut[Port] - DimSteps;
    }
    
    if (StateOut[Port] < 0 ) StateOut[Port] = 0;
    SetPort(Port, StateOut[Port]);
    delay(20);
  }
}

void TogglePort(byte Port){
  Serialout("TogglePort " + String(Port));
  if (StateOut[Port] > 0){
    DimDown(Port);
  }else{
    DimUp(Port);
  }
}


void handleRoot() {
  byte ch = (char)server.arg("ch").toInt();
  String op = server.arg("op");
  
  String Info;
  Info += "<b>Dimmer</b><br><hr>";
  Info += "<b>Kanal 1 </b>  <a href='/?ch=1&op=dunkler'>dunker</a>&nbsp;<a href='/?ch=1&op=toggle'>Toggeln</a>&nbsp;<a href='/?ch=1&op=heller'>heller</a><br>";
  Info += "<b>Kanal 2 </b>  <a href='/?ch=2&op=dunkler'>dunker</a>&nbsp;<a href='/?ch=2&op=toggle'>Toggeln</a>&nbsp;<a href='/?ch=2&op=heller'>heller</a><br>";
  Info += "<b>Kanal 2 </b>  <a href='/?ch=3&op=dunkler'>dunker</a>&nbsp;<a href='/?ch=3&op=toggle'>Toggeln</a>&nbsp;<a href='/?ch=3&op=heller'>heller</a><br>";

  Info += "ch: " + String(ch) + " op: "+op;
 
  server.send(200, "text/html", Info);
 
  if (op == "toggle") TogglePort(ch);
  if (op == "heller") Dim(ch, op);
  if (op == "dunkler") Dim(ch, op);
}

void setup() {
  
  Serial.begin(115200);
  while (!Serial);
  {
  }
  Serial.println("");
  EEPROM.begin(4096);

  pinMode(4, INPUT);
  pinMode(2, INPUT);
  pinMode(0, INPUT);

  pinMode(12, OUTPUT);
  pinMode(13, OUTPUT);
  pinMode(15, OUTPUT);

  SetPort(1, 0);
  SetPort(2, 0);
  SetPort(3, 0);

  analogWriteFreq(31300);
    WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  
  //WiFi.softAP("Dimmer Kueche", "Dikt81mp1");

  
  httpUpdater.setup(&server);
  server.on("/", handleRoot);
  server.begin();

}

void loop() {
  server.handleClient();
  
  checkPin(1);
  checkPin(2);
  checkPin(3);
}
