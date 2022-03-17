#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <PubSubClient.h>

#define MQTT_VERSION MQTT_VERSION_3_1_1

const PROGMEM char *ap_ssid = "nightlight";
const PROGMEM char *ap_password = ""; // No password

char sta_ssid[32] = {0};
char sta_password[64] = {0};

const PROGMEM char *webpage_html = "\
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

IPAddress local_IP(192, 168, 0, 1);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);

void initApConfig();
void initWebServer();
void connectToWifi();
void handleRootPost();
void handleRoot();
void handleNotFound(); // web part

void readWifiInfo();
void saveWifiInfo(uint8_t wifiWork, char sta_ssid[32], char sta_password[64]);
void EEPROMUpdate(int const address, uint8_t const value); // wifi part
const uint8_t EEPROM_SIZE = 128;

void publishLightState();
void publishLightBrightness();
void callback(char *p_topic, byte *p_payload, unsigned int p_length);
void connectToMQTT();
void setBrightness();
unsigned long reconnectTime = 0;

const PROGMEM char *MQTT_CLIENT_ID = "bedroom_night_light";
const PROGMEM char *MQTT_SERVER_IP = "192.168.31.128";
const PROGMEM uint16_t MQTT_SERVER_PORT = 1883;
const PROGMEM char *MQTT_USER = "UserOfMQTT";
const PROGMEM char *MQTT_PASSWORD = "mqtt1234";

// MQTT: topics
const PROGMEM char *MQTT_LIGHT_STATE_TOPIC = "bedroom/light/status";
const PROGMEM char *MQTT_LIGHT_COMMAND_TOPIC = "bedroom/light/switch";
// state
const PROGMEM char *MQTT_LIGHT_BRIGHTNESS_STATE_TOPIC = "bedroom/brightness/status";
const PROGMEM char *MQTT_LIGHT_BRIGHTNESS_COMMAND_TOPIC = "bedroom/brightness/set";
// brightness

// sleep mode
// not finied yet

boolean light_state = false;
uint8_t light_brightness = 100;
// variables used to store the state and the brightness
const uint8_t MSG_BUFFER_SIZE = 20;
char msg_buffer[MSG_BUFFER_SIZE];
// buffer used to send/receive data with MQTT
const PROGMEM char *LIGHT_ON = "ON";
const PROGMEM char *LIGHT_OFF = "OFF";
// payloads by default (on/off)
#define LIGHT_PIN 0

// light pin

WiFiClient wifiClient;
PubSubClient client(wifiClient);

ESP8266WebServer server(80);

void setup(void)
{
  pinMode(LIGHT_PIN, OUTPUT);
  Serial.begin(9600);
  EEPROM.begin(EEPROM_SIZE);
  Serial.println();

  if (EEPROM.read(0) != 1)
  {
    initApConfig();
    initWebServer();
    Serial.printf("Open http://%s in your browser\n", WiFi.softAPIP().toString().c_str());
  }
  else
  {
    readWifiInfo();
    connectToWifi();
  }

  pinMode(LIGHT_PIN, OUTPUT);
  client.setServer(MQTT_SERVER_IP, MQTT_SERVER_PORT);
  client.setCallback(callback);
}

void loop(void)
{
  server.handleClient();
  if (!client.connected())
  {
    connectToMQTT();
  }
  client.loop();
}

void initApConfig()
{
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(ap_ssid, ap_password);
}

void initWebServer()
{
  server.on("/", HTTP_GET, handleRoot);
  server.on("/", HTTP_POST, handleRootPost);
  server.onNotFound(handleNotFound);

  server.begin();
}

void connectToWifi()
{
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(sta_ssid, sta_password);

  int waittingTime = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    waittingTime++;
    Serial.print(".");
    if (waittingTime >= 40)
    {
      waittingTime = 0;
      EEPROMUpdate(0, 0); // set wifi as not available
      EEPROM.commit();
      ESP.restart();
    }
  }
}

void handleRootPost()
{
  if (server.hasArg("ssid"))
  {
    strcpy(sta_ssid, server.arg("ssid").c_str());
  }
  else
  {
    server.send(200, "text/html", "<meta charset='UTF-8'>error, ssid not found ");
    return;
  }

  if (server.hasArg("password"))
  {
    strcpy(sta_password, server.arg("password").c_str());
  }
  else
  {
    server.send(200, "text/html", "<meta charset='UTF-8'>error, Password not found ");
    return;
  }

  server.send(200, "text/html", "<meta charset='UTF-8'>Saved!");
  delay(2000);

  saveWifiInfo(1, sta_ssid, sta_password);

  connectToWifi();
}

void handleRoot()
{
  server.send(200, "text/html", webpage_html);
}

void handleNotFound()
{
  String message = "File Not Found\n\n";
  server.send(404, "text/plain", message);
}

void saveWifiInfo(uint8_t wifiWork, char sta_ssid[32], char sta_password[64])
{
  EEPROMUpdate(0, wifiWork);

  EEPROMUpdate(1, (byte)strlen(sta_ssid)); // write the length of ssid

  byte index = 2;
  for (int i = 0; i < (int)strlen(sta_ssid); i++)
  {
    EEPROMUpdate(index, sta_ssid[i]);
    index++;
  } // write ssid

  EEPROMUpdate(index, (byte)strlen(sta_password)); // write the length of password
  index++;

  for (int i = 0; i < (int)strlen(sta_password); i++)
  {
    EEPROMUpdate(index, sta_password[i]);
    index++;
  } // write password

  EEPROM.commit();
}

void readWifiInfo()
{
  byte length = EEPROM.read(1);

  byte index1 = 2;
  while (index1 < length + 2)
  {
    sta_ssid[index1 - 2] = EEPROM.read(index1);
    index1++;
  }
  length = EEPROM.read(index1);

  index1++;
  byte index2 = 0;
  while (index2 < length)
  {
    sta_password[index2] = EEPROM.read(index1);
    index2++;
    index1++;
  }
}

void EEPROMUpdate(int const address, uint8_t const value)
{
  /*
  It seems like EEPROM.h in ESP8266 is a decrepit version, does not like the same library's in arduino
  This function is used to replace the update() in arduino's EEPROM.h
  */
  if (EEPROM.read(address) != value)
  {
    EEPROM.write(address, value);
  }
}

void publishLightState()
{
  if (light_state)
  { // function called to publish the state of the led (on/off)
    client.publish(MQTT_LIGHT_STATE_TOPIC, LIGHT_ON, true);
    digitalWrite(LIGHT_PIN, 1);
  }
  else
  {
    client.publish(MQTT_LIGHT_STATE_TOPIC, LIGHT_OFF, true);
    digitalWrite(LIGHT_PIN, 0);
  }
}

void publishLightBrightness()
{
  // function called to publish the brightness of the led (0-100)
  snprintf(msg_buffer, MSG_BUFFER_SIZE, "%d", light_brightness);
  client.publish(MQTT_LIGHT_BRIGHTNESS_STATE_TOPIC, msg_buffer, true);
}

void connectToMQTT()
{
  // Loop until connection lost
  while (!client.connected())
  {
    Serial.println("connect to MQTT server");
    // try to connect
    if (millis() - reconnectTime < 0)
    { // if internal timer(millis) leaked and reset after 50 days
      reconnectTime = 0;
    }
    if (millis() - reconnectTime > 3000)
    {
      if (client.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD))
      {
        Serial.println("connected");

        // Once connected, publish an announcement...
        // publish the initial values
        publishLightState();
        publishLightBrightness();

        // ... and resubscribe
        client.subscribe(MQTT_LIGHT_COMMAND_TOPIC);
        client.subscribe(MQTT_LIGHT_BRIGHTNESS_COMMAND_TOPIC);
      }
      else
      {
        Serial.print("Error:");
        Serial.println(client.state());
        Serial.println("reconnect in 3 seconds");
        reconnectTime = millis();
      }
    }
  }
}

void callback(char *p_topic, byte *p_payload, unsigned int p_length)
{
  // concat the payload into a string
  String payload;
  for (uint8_t i = 0; i < p_length; i++)
  {
    payload.concat((char)p_payload[i]);
  }
  // handle message topic
  if (String(MQTT_LIGHT_COMMAND_TOPIC).equals(p_topic))
  {
    // open the light or turn it off
    if (payload.equals(String(LIGHT_ON)))
    {
      if (light_state != true)
      {
        light_state = true;
        publishLightState();
      }
    }
    else if (payload.equals(String(LIGHT_OFF)))
    {
      if (light_state != false)
      {
        light_state = false;
        publishLightState();
      }
    }
  }
  else if (String(MQTT_LIGHT_BRIGHTNESS_COMMAND_TOPIC).equals(p_topic))
  {
    uint8_t brightness = payload.toInt();
    if (brightness > 0 && brightness < 100)
    {
      light_brightness = brightness;
      setBrightness();
      publishLightBrightness();
    }
  }
}

void setBrightness()
{
  if (light_state)
  {
    analogWrite(LIGHT_PIN, map(light_brightness, 0, 100, 0, 255));
  }
}
