#include <EEPROM.h>
#include <ArduinoOTA.h>
#include <WiFiClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266HTTPUpdateServer.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESP8266WiFiMulti.h>

HTTPClient sender;
WiFiClient wifiClient;

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
ESP8266WiFiMulti wifiMulti;

PubSubClient MqttClient(wifiClient);

extern "C" {
#include "user_interface.h"
}
extern "C" {
#include "gpio.h"
}



wifiMulti.addAP("HILDI", "Dikt81mp1!");
wifiMulti.addAP("K2-NET", "Dikt81mp!");
boolean connectioWasAlive = true;

const char* MQTT_BROKER = "192.168.8.1";
const char* MQTT_Prefix = "Hildi/Control/";

StaticJsonDocument<200> doc;

String Name = "DimmerUnknown";
bool debug = false;
bool isOffline = false;
byte DimSteps = 5;
byte PortsIn[] = {0, 4, 0, 2};
byte PortsInState[] = {1, 1, 1, 1};
long PortsInStateChange[] = {0, 0, 0, 0};

byte PortsOut[] = {0, 12, 13, 15};
byte StateOut[] = {0, 0, 0, 0};
long IsDimming[] = {0, 0, 0, 0};
String NextDimAction[] = {"null", "null", "null", "null"};
String KanalName[] = {"CH1", "CH2", "CH3", "CH4"};

void handleJson() {
  char json[] = "{\"sensor\":\"gps\",\"time\":1351824120,\"data\":[48.756080,2.302038]}";
  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }
  const char* sensor = doc["sensor"];
  long time = doc["time"];
  double latitude = doc["data"][0];
  double longitude = doc["data"][1];

  // Print values.
  Serial.println(sensor);
  Serial.println(time);
  Serial.println(latitude, 6);
  Serial.println(longitude, 6);

}
void SetPort(byte Port, byte value) {
  //Serialout("SetPort " + String(Port) + " to " + value);
  StateOut[Port] = value;
  analogWrite(PortsOut[Port], (1023 * value) / 100);
}
void Serialout(String Message) {
  Serial.println(Message);
}
void checkPin(byte Port) {
  int inputRead = digitalRead(PortsIn[Port]);

  if (inputRead != PortsInState[Port]) {
    // state changed
    PortsInState[Port] = inputRead;
    PortsInStateChange[Port] = millis();

    if (inputRead == 0) {
      // warten wenn an
      delay(400);
      // wenn dann aus,
      if (digitalRead(PortsIn[Port]) == 1) {
        // war es ein Pulse !!
        if (millis() - IsDimming[Port] > 1000) TogglePort(Port);
      }
    }
  } else {
    if (inputRead == 0) {
      if ((millis() - PortsInStateChange[Port] > 600)) Dim(Port);
    }
  }
}
void Dim(byte Port) {

  if (NextDimAction[Port] == "null") {
    if (StateOut[Port] > 0) {
      //Serialout("first dim for " + String(Port) + " (down)");
      NextDimAction[Port] = "down";
    } else {
      //Serialout("first dim for " + String(Port) + " (up)");
      NextDimAction[Port] = "up";
    }
  }

  IsDimming[Port] = millis();
  if (NextDimAction[Port] == "up") {
    Serialout("Dim up " + String(Port));
    if (StateOut[Port] < 10) {
      StateOut[Port] = StateOut[Port] + 1;
    } else {
      StateOut[Port] = StateOut[Port] + DimSteps;
    }
    if (StateOut[Port] > 100 ) {
      StateOut[Port] = 100;
      NextDimAction[Port] = "down";
    }

  } else {
    //Serialout("Dim down " + String(Port));
    if (StateOut[Port] < 10) {
      StateOut[Port] = StateOut[Port] - 1;
    } else {
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
void DimTo(byte Port, byte valueEnd) {
  Serialout("DimTo Port " + String(Port) + " from " + String(StateOut[Port]) + " to " + String(valueEnd));
  byte targetVavlue = 0;
  if (StateOut[Port] > valueEnd) {
    // dimdown
    while (StateOut[Port] > valueEnd) {
      targetVavlue = StateOut[Port] - 1;
      SetPort(Port, targetVavlue);
      delay(5);
    }
  } else {
    // dimup
    while (StateOut[Port] < valueEnd) {
      targetVavlue = StateOut[Port] + 1;
      SetPort(Port, targetVavlue);
      delay(5);
    }
  }
}
void Dim(byte Port, String dir) {
  if (dir == "heller") {
    StateOut[Port] = StateOut[Port] + DimSteps;
  } else {
    StateOut[Port] = StateOut[Port] - DimSteps;
  }
  SetPort(Port, StateOut[Port]);
}
void DimUp(byte Port) {
  Serialout("DimUp " + String(Port));
  while (StateOut[Port] < 100) {
    StateOut[Port] = StateOut[Port] + DimSteps;
    if (StateOut[Port] > 100 ) StateOut[Port] = 100;
    SetPort(Port, StateOut[Port]);
    delay(20);
  }
}
void DimDown(byte Port) {
  Serialout("DimDown " + String(Port));
  while (StateOut[Port] > 0) {

    if (StateOut[Port] < DimSteps ) {
      StateOut[Port] = 0;
    } else {
      StateOut[Port] = StateOut[Port] - DimSteps;
    }

    if (StateOut[Port] < 0 ) StateOut[Port] = 0;
    SetPort(Port, StateOut[Port]);
    delay(20);
  }
}
void TogglePort(byte Port) {
  Serialout("TogglePort " + String(Port));
  
 // MqttPublish("Hildi/Control/"+Name+"/"+String(Port)+"/TogglePort/", "");

  if (StateOut[Port] > 0) {
    DimDown(Port);
  } else {
    DimUp(Port);
  }
}

void handleSet() {
  byte ch = (char)server.arg("ch").toInt();
  byte value = (char)server.arg("value").toInt();
  DimTo(ch, value);
  server.send(200, "text/json", GetInfo());
}
void handleGet() {
  server.send(200, "text/json", GetInfo());
}
String GetInfo(){
  return "{ \"device\": \""+Name+"\", \"channels\": [" + GetChInfo(1) + ", " + GetChInfo(2) + ", " + GetChInfo(3) + ", " + GetChInfo(4) + "]}";
}
String GetChInfo(byte ch){
  return "{\"id\": " + String(ch) + ", \"dir\": \"out\" ,\"name\": \"" + KanalName[ch] + "\", \"value\": " + String(StateOut[ch]) + "}";
}
void handleRoot() {
  byte ch = (char)server.arg("fan").toInt();
  byte fanval = (char)server.arg("fanval").toInt();
  String op = server.arg("op");

  if (op == "99") debug = true;
  if (op == "1") TogglePort(ch);
  if (op == "3") DimTo(ch, fanval);

  String result = "<html>\n<head>\n<meta http-equiv=\"content-type\" content=\"text/html;charset=UTF-8\">\n<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n<link rel=\"stylesheet\" href=\"https://getbootstrap.com/docs/4.1/dist/css/bootstrap.min.css\">\n<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/3.3.1/jquery.min.js\"></script>\n<script src=\"https://getbootstrap.com/docs/4.1/dist/js/bootstrap.min.js\"></script>\n<title>FreezerControl</title>\n</head>\n\n<body>\n<nav class=\"navbar navbar-expand-md navbar-dark bg-dark\">\n<div class=\"navbar-collapse collapse w-100 order-1 order-md-0 dual-collapse2\"><ul class=\"navbar-nav mr-auto\">";
  // links linke seite
  result += "<li class=\"nav-item active\">\n<a class=\"nav-link\" href=\"http://192.168.8.10\">Logger</a>\n</li>";
  // Titel
  result += "</ul></div>\n\n<div class=\"mx-auto order-0\"><a class=\"navbar-brand mx-auto\" href=\"\">" + Name + "</a><button class=\"navbar-toggler\" type=\"button\" data-toggle=\"collapse\" data-target=\".dual-collapse2\"><span class=\"navbar-toggler-icon\"></span></button></div>\n    \n    <div class=\"navbar-collapse collapse w-100 order-3 dual-collapse2\">\n         <ul class=\"navbar-nav ml-auto\">";
  // links rechte seite
  result += "<li><a class=\"nav-link\" href=\"http://192.168.8.12/\">Kabine</a></li>\n";    
  result += "<li><a class=\"nav-link\" href=\"http://192.168.8.13/\">Bad</a></li>\n\t";
  result += "<li><a class=\"nav-link\" href=\"http://192.168.8.14/\">Außen</a></li>\n\t"; 
 //result += "<li><hr class=\"dropdown-divider\"></li>"; 
  result += "<li><a class=\"nav-link\" href=\"http://192.168.8.11/\">Kuehlschrank</a></li>";
  
  // navi ende
  result += "</ul></div></nav><div class=\"container\"><br>";

  result += "<form id=\"setFanForm\" method='POST'>";
  result += "<input type='hidden' name='op' id='op'>";
  result += "<input type='hidden' name='fan' id='fan'>";
  result += "<input type='hidden' name='fanval' id='fanval'>";
  result += "</form>";

  result += "<script> function SetFan(op, fanNumber, fanVal) {document.getElementById('op').value = op; document.getElementById('fan').value = fanNumber;document.getElementById('fanval').value=fanVal; document.getElementById(\"setFanForm\").submit();} </script>";

  result += "<div  style=\"width:600px;\">";
  result += "<button type=\"button\" class=\"btn btn-secondary btn-sm\" onclick=\"SetFan(1,1,0)\">Toggle</button>&nbsp;";
  result += "<button type=\"button\" class=\"btn btn-secondary btn-sm\" onclick=\"SetFan(3,1,0)\">OFF</button>&nbsp;";
  result += "<input type=\"range\" class=\"form-range\" id=\"customRange1\" value='" + String(StateOut[1]) + "' min=\"0\" max=\"100\" step=\"1\" onInput=\"SetFan(3,1,this.value)\" style=\"width:400px;\">&nbsp;";
  result += "<button type=\"button\" class=\"btn btn-secondary btn-sm\" onclick=\"SetFan(3,1,100)\">ON</button>&nbsp;";
  result += "</div></br>";

  result += "<div  style=\"width:600px;\">";
  result += "<button type=\"button\" class=\"btn btn-secondary btn-sm\" onclick=\"SetFan(1,2,0)\">Toggle</button>&nbsp;";
  result += "<button type=\"button\" class=\"btn btn-secondary btn-sm\" onclick=\"SetFan(3,2,0)\">OFF</button>&nbsp;";
  result += "<input type=\"range\" class=\"form-range\" id=\"customRange1\" value='" + String(StateOut[2]) + "' min=\"0\" max=\"100\" step=\"1\" onInput=\"SetFan(3,2,this.value)\" style=\"width:400px;\">&nbsp;";
  result += "<button type=\"button\" class=\"btn btn-secondary btn-sm\" onclick=\"SetFan(3,2,100)\">ON</button>&nbsp;";
  result += "</div></br>";

  result += "<div  style=\"width:600px;\">";
  result += "<button type=\"button\" class=\"btn btn-secondary btn-sm\" onclick=\"SetFan(1,3,0)\">Toggle</button>&nbsp;";
  result += "<button type=\"button\" class=\"btn btn-secondary btn-sm\" onclick=\"SetFan(3,3,0)\">OFF</button>&nbsp;";
  result += "<input type=\"range\" class=\"form-range\" id=\"customRange1\" value='" + String(StateOut[3]) + "' min=\"0\" max=\"100\" step=\"1\" onInput=\"SetFan(3,3,this.value)\" style=\"width:400px;\">&nbsp;";
  result += "<button type=\"button\" class=\"btn btn-secondary btn-sm\" onclick=\"SetFan(3,3,100)\">ON</button>&nbsp;";
  result += "</div></br>";

  if (debug){
    for (int i = 0; i < server.args(); i++) {
      result += "<br><div>Arg " + String(i) + ":  [" + server.argName(i) + "]->[" + server.arg(i) + "]</div>";
    }
  }
  


  String Info;
  Info += "<b>Dimmer</b><br><hr>";
  Info += "<b>Kanal 1 </b>  <a href='/?ch=1&op=dunkler'>dunker</a>&nbsp;<a href='/?ch=1&op=toggle'>Toggeln</a>&nbsp;<a href='/?ch=1&op=heller'>heller</a><br>";
  Info += "<b>Kanal 2 </b>  <a href='/?ch=2&op=dunkler'>dunker</a>&nbsp;<a href='/?ch=2&op=toggle'>Toggeln</a>&nbsp;<a href='/?ch=2&op=heller'>heller</a><br>";
  Info += "<b>Kanal 2 </b>  <a href='/?ch=3&op=dunkler'>dunker</a>&nbsp;<a href='/?ch=3&op=toggle'>Toggeln</a>&nbsp;<a href='/?ch=3&op=heller'>heller</a><br>";
  Info += "ch: " + String(ch) + " op: " + op;

  server.send(200, "text/html", result);

}

void LogToApi(String Ident, String message) {

  if (isOffline) return;

  String url = "http://192.168.8.10/write/?I=" + Ident + "&D=" + message;

  if (sender.begin(wifiClient, url)) {
    // HTTP-Code der Response speichern
    int httpCode = sender.GET();
    if (httpCode > 0) {

      // Anfrage wurde gesendet und Server hat geantwortet
      // Info: Der HTTP-Code für 'OK' ist 200
      if (httpCode == HTTP_CODE_OK) {

        // Hier wurden die Daten vom Server empfangen

        // String vom Webseiteninhalt speichern
        String payload = sender.getString();

        // Hier kann mit dem Wert weitergearbeitet werden
        // ist aber nicht unbedingt notwendig
        Serial.println(payload);
      }
    } else {
      // Falls HTTP-Error
      Serial.printf("HTTP-Error: ", sender.errorToString(httpCode).c_str());
    }

    // Wenn alles abgeschlossen ist, wird die Verbindung wieder beendet
    sender.end();

  } else {
    Serial.printf("HTTP-Verbindung konnte nicht hergestellt werden!");
  }

  delay(100);
}
void Syslog(String message){
  LogToApi(Name, message);
}

void MqttSetup() {
  MqttClient.setServer(MQTT_BROKER, 1883);
  MqttClient.setCallback(MqttCallback);
}
void MqttPublish(String payload) {

  String topic = String(MQTT_Prefix)+ Name + "/TX";

  if (MqttClient.connected()) {
      MqttClient.publish(topic.c_str(), payload.c_str() );
    }else{
      Serial.println("MqttClient not connected");
    }
}
void MqttCallback(char* topic, byte* payload, unsigned int length) {
    String msg;
    for (byte i = 0; i < length; i++) {
        char tmp = char(payload[i]);
        msg += tmp;
    }
    Serial.println(msg);
}
void MqttCheckConnection() {
  if (!MqttClient.connected()) {
    while (!MqttClient.connected()) {
      MqttClient.connect("ESP8266Client");
      String topic = String(MQTT_Prefix)+ Name + "/RX";
      MqttClient.subscribe(topic.c_str());
      Serial.println("MQTT Topic for recive: " + topic);
      MqttPublish("Hallo Welt!");
      delay(100);
    }
  }
}
void MqttLoop() {
  MqttClient.loop();
}

void monitorWiFi()
{
  if (wifiMulti.run() != WL_CONNECTED)
  {
    if (connectioWasAlive == true)
    {
      connectioWasAlive = false;
      Serial.println("Looking for WiFi ");
    }
    Serial.print(".");
    delay(500);
  }
  else if (connectioWasAlive == false)
  {
    connectioWasAlive = true;
    Serial.printf(" connected to %s\n", WiFi.SSID().c_str());
  }



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

  monitorWiFi();

 }
 if (!isOffline) {
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  String ipaddr = WiFi.localIP().toString();
  Serial.println("MyIp: " + ipaddr);

  if (ipaddr == "192.168.8.11") {
    Name = "K&uuml;hlschrank";
    KanalName[1] = "CH1";
    KanalName[2] = "CH2";
    KanalName[3] = "CH3";
  }
  else if (ipaddr == "192.168.8.12") {
    Name = "Dimmer Kabine";
    KanalName[1] = "CH1";
    KanalName[2] = "CH2";
    KanalName[3] = "CH3";
  }
  else if (ipaddr == "192.168.8.13") {
    Name = "Dimmer Bad";
    KanalName[1] = "CH1";
    KanalName[2] = "CH2";
    KanalName[3] = "CH3";
  }
  else if (ipaddr == "192.168.8.14") {
    Name = "Dimmer Außen";
    KanalName[1] = "CH1";
    KanalName[2] = "CH2";
    KanalName[3] = "CH3";
  }
  
  // webserver
  httpUpdater.setup(&server);
  server.on("/", handleRoot);
  server.on("/set/", handleSet);
  server.on("/get/", handleGet);
  server.begin();
  // MQTT
  MqttSetup();

  } else{
    Serial.println("Offline!!");
  }

  Serial.println("Hello, I´m " + Name);
  handleJson();
}

void loop() {
  monitorWiFi();
  if (!isOffline) {
    server.handleClient();

    MqttCheckConnection();
    MqttLoop();
  }

 // checkPin(1);
 // checkPin(2);
 // checkPin(3);
}
