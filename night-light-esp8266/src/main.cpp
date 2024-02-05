#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <PubSubClient.h>
#include <DNSServer.h>

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
  <style>\r\n\
    body {\r\n\
      display: flex;\r\n\
      justify-content: center;\r\n\
      align-items: center;\r\n\
      height: 100%;\r\n\
    }\r\n\
  </style>\r\n\
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

DNSServer dnsServer;
const byte DNS_PORT = 53;
IPAddress local_IP(192, 168, 0, 1);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);
ESP8266WebServer server(80);

void initApConfig();
void initWebServer();
void connectToWifi();
void handleRootPost();
void handleRoot();
void handleNotFound();
void initDNS();
// web part

void readWifiInfo();
void saveWifiInfo(uint8_t wifiWork, char sta_ssid[32], char sta_password[64]);
void EEPROMUpdate(int const address, uint8_t const value); // wifi part
const uint8_t EEPROM_SIZE = 128;

IRAM_ATTR void publishLightState();
IRAM_ATTR void ButtonControl();
void publishLightBrightness();
void callback(char *p_topic, byte *p_payload, unsigned int p_length);
void connectToMQTT();
void setBrightness();
unsigned long reconnectTime = 0;

void breathingWhileConnecting();
byte brightnessForBreathing = 0;
bool acsendOrDecend = false;
void blink(int speed);

const PROGMEM char *MQTT_CLIENT_ID = "ESP_Bedroom_Night_Light";
const PROGMEM char *MQTT_SERVER_IP = "192.168.31.178";
const PROGMEM uint16_t MQTT_SERVER_PORT = 1883;
const PROGMEM char *MQTT_USER = "mqtt_sender";
const PROGMEM char *MQTT_PASSWORD = "1234";

// MQTT: topics
const PROGMEM char *MQTT_LIGHT_STATE_TOPIC = "bedroom/nightLight/status";
const PROGMEM char *MQTT_LIGHT_COMMAND_TOPIC = "bedroom/nightLight/switch";
// state
const PROGMEM char *MQTT_LIGHT_BRIGHTNESS_STATE_TOPIC = "bedroom/nightLight/brightness/status";
const PROGMEM char *MQTT_LIGHT_BRIGHTNESS_COMMAND_TOPIC = "bedroom/nightLight/brightness/set";
// brightness

// sleep mode
// not finied yet

volatile boolean light_state = false;
uint8_t light_brightness = 255;
// variables used to store the state and the brightness
const uint8_t MSG_BUFFER_SIZE = 20;
char msg_buffer[MSG_BUFFER_SIZE];
// buffer used to send/receive data with MQTT
const PROGMEM char *LIGHT_ON = "ON";
const PROGMEM char *LIGHT_OFF = "OFF";
// payloads by default (on/off)
#define LIGHT_PIN 0
#define BUTTON_PIN 2
// light pin & BUTTON_PIN
volatile boolean light_button_delay = false;
volatile long button_delay_time = 0;
#define Debouncing_delay 200

WiFiClient wifiClient;
PubSubClient client(wifiClient);

void setup(void)
{
  pinMode(LIGHT_PIN, INPUT_PULLUP);

  Serial.begin(9600);
  EEPROM.begin(EEPROM_SIZE);
  Serial.println();

  pinMode(LIGHT_PIN, OUTPUT);
  digitalWrite(LIGHT_PIN, 1);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), ButtonControl, FALLING);

  if (EEPROM.read(0) != 1)
  {
    initApConfig();
    initWebServer();
    initDNS();
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
  dnsServer.processNextRequest();
  if (WiFi.status() == WL_CONNECTED)
  {
    if (!client.connected())
    {
      connectToMQTT();
    }
  }
  else
  {
    // connectToWifi();
  }
  // breathingWhileConnecting();
  //delay(100);
  client.loop();
}

void initApConfig()
{
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_IP, gateway, IPAddress(subnet));
  WiFi.softAP(ap_ssid, ap_password);
}

void initWebServer()
{
  server.on("/", HTTP_GET, handleRoot);
  server.onNotFound(handleRoot);
  server.on("/", HTTP_POST, handleRootPost);
  server.begin();
}

void initDNS()
{
  if (dnsServer.start(DNS_PORT, "*", local_IP))
  {
    Serial.println("dnsServer initialated");
  }
}

void connectToWifi()
{
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(sta_ssid, sta_password);
  WiFi.setHostname(MQTT_CLIENT_ID);

  int waittingTime = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(100);
    waittingTime++;
    Serial.print(".");
    breathingWhileConnecting();
    if (waittingTime >= 1000)
    {
      waittingTime = 0;
      EEPROMUpdate(0, 0); // set wifi as not available
      EEPROM.commit();
      ESP.restart();
    }
  }
  digitalWrite(LIGHT_PIN, 1);
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
  handleRoot(); // still using handleRoot even not found, or the captive portal would failed
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

IRAM_ATTR void publishLightState()
{
  if (light_state)
  { // function called to publish the state of the led (on/off)
    client.publish(MQTT_LIGHT_STATE_TOPIC, LIGHT_ON, true);
    setBrightness();
  }
  else
  {
    client.publish(MQTT_LIGHT_STATE_TOPIC, LIGHT_OFF, true);
    digitalWrite(LIGHT_PIN, 1);
  }
}

void publishLightBrightness()
{
  // function called to publish the brightness of the led (0-255)
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
        // blink(200);
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
    if (brightness > 0 && brightness < 255)
    {
      light_brightness = brightness;
      Serial.println(light_brightness);
      setBrightness();
      publishLightBrightness();
    }
  }
}

void setBrightness()
{
  if (light_state)
  {
    analogWrite(LIGHT_PIN, 255 - light_brightness);
    if (light_brightness > 242)//if brightness is over 95%. then use the digital write
    {
      digitalWrite(LIGHT_PIN, !light_state);
    }
    
  }
}

IRAM_ATTR void ButtonControl()
{
  if ((millis() - button_delay_time) < 0)
  {
    button_delay_time = 0;
  }
  if ((millis() - button_delay_time) > Debouncing_delay)
  {
    button_delay_time = millis();
    light_state = !light_state;
    publishLightState();
  }
}

void breathingWhileConnecting()
{
  if ((brightnessForBreathing == 255) || (brightnessForBreathing == 0))
  {
    acsendOrDecend = !acsendOrDecend;
  }

  analogWrite(LIGHT_PIN, 255 - brightnessForBreathing);
  if (acsendOrDecend)
  {
    brightnessForBreathing += 5;
    Serial.println(brightnessForBreathing);
  }
  else
  {
    brightnessForBreathing -= 5;
    Serial.println(brightnessForBreathing);
  }
}

void blink(int speed)
{
  digitalWrite(LIGHT_PIN, 0);
  delay(speed);
  digitalWrite(LIGHT_PIN, 1);
  delay(speed);
}