#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <PubSubClient.h>

#define MQTT_VERSION MQTT_VERSION_3_1_1

const char* ap_ssid = "nightlight";
const char* ap_password = "";//No password

char sta_ssid[32] = {0};
char sta_password[64] = {0};

const char* webpage_html = "\
<!DOCTYPE html>\r\n\
<html lang='en'>\r\n\
<head>\r\n\
  <meta charset='UTF-8'>\r\n\
  <title>Document</title>\r\n\
</head>\r\n\
<body>\r\n\
  <form name='input' action='/' method='POST'>\r\n\
        Wifi Name: <br>\r\n\
        <input type='text' name='ssid'><br>\r\n\
        Wifi Password:<br>\r\n\
        <input type='password' name='password'><br>\r\n\
        <input type='submit' value='Save'>\r\n\
    </form>\r\n\
</body>\r\n\
</html>\r\n\
";

IPAddress local_IP(192,168,0,1);
IPAddress gateway(192,168,0,1);
IPAddress subnet(255,255,255,0);

void initApConfig();
void initWebServer();
void connectToWifi();
void handleRootPost();
void handleRoot();
void handleNotFound();

void readWifiInfo();
void saveWifiInfo(byte wifiWork, char sta_ssid[32], char sta_password[64]);

void EEPROMUpdate(int const address, uint8_t const value);


ESP8266WebServer server(80);

void setup(void) {
  Serial.begin(9600);
  EEPROM.begin(128);
  for(int i = 0; i < 25; i++){
    Serial.print(EEPROM.read(i));
    Serial.print(" ");
  }
  Serial.println();

  if(EEPROM.read(0) != 1){
    initApConfig();
    initWebServer();
    Serial.printf("Open http://%s in your browser\n", WiFi.softAPIP().toString().c_str());
  }else{
    readWifiInfo();
    connectToWifi();
  }
  
  

  
}

void loop(void) {
  server.handleClient();
}

void initApConfig(){
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(ap_ssid, ap_password);
}

void initWebServer(){
  server.on("/", HTTP_GET, handleRoot);
  server.on("/", HTTP_POST, handleRootPost);
  server.onNotFound(handleNotFound);

  server.begin();   
}

void connectToWifi(){
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(sta_ssid, sta_password);

  int waittingTime = 0;
  while (WiFi.status() != WL_CONNECTED) {
          delay(500);
          waittingTime++;
          Serial.print(".");
          if(waittingTime >= 40){
            waittingTime = 0;
            EEPROMUpdate(0, 0);//set wifi as not available
            EEPROM.commit();
            ESP.restart();
          }
  }
}

void handleRootPost() {
  if (server.hasArg("ssid")) {
    strcpy(sta_ssid, server.arg("ssid").c_str());
  } else {
    server.send(200, "text/html", "<meta charset='UTF-8'>error, ssid not found ");
    return;
  }

  if (server.hasArg("password")) {
    strcpy(sta_password, server.arg("password").c_str());
  } else {
    server.send(200, "text/html", "<meta charset='UTF-8'>error, Password not found ");
    return;
  }

  server.send(200, "text/html", "<meta charset='UTF-8'>Saved!");
  delay(2000);

  saveWifiInfo(1, sta_ssid, sta_password);

  connectToWifi();
}

void handleRoot() {
  server.send(200, "text/html", webpage_html);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  server.send(404, "text/plain", message);
}

void saveWifiInfo(byte wifiWork, char sta_ssid[32], char sta_password[64]){
  EEPROMUpdate(0, wifiWork);

  EEPROMUpdate(1, (byte)strlen(sta_ssid));//write the length of ssid

  byte index = 2;
  for(int i = 0; i < (int)strlen(sta_ssid); i ++){
    EEPROMUpdate(index, sta_ssid[i]);
    index ++;
  }//write ssid

  EEPROMUpdate(index, (byte)strlen(sta_password));//write the length of password
  index ++;

  for(int i = 0; i < (int)strlen(sta_password); i ++){
    EEPROMUpdate(index, sta_password[i]);
    index ++;
  }//write password

  EEPROM.commit();
}

void readWifiInfo(){
  byte length = EEPROM.read(1);

  byte index1 = 2;
  while (index1 < length + 2)
  {
    sta_ssid[index1 - 2] = EEPROM.read(index1);
    index1 ++;
  }
  length = EEPROM.read(index1);

  index1 ++;
  byte index2 = 0;
  while (index2 < length){
    sta_password[index2] = EEPROM.read(index1);
    index2 ++;
    index1 ++;
  }
  
  
}

void EEPROMUpdate(int const address, uint8_t const value){
  /*
  It seems like EEPROM.h in ESP8266 is a decrepit version, does not like the same library's in arduino
  This function is used to replace the update() in arduino's EEPROM.h
  */
  if(EEPROM.read(address) != value){
    EEPROM.write(address, value);
  }
}