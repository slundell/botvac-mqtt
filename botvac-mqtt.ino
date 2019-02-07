#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>

// Update with suitable values
//#define MAX_BUFFER 8192
#define SSID "yourwifinetwork"
#define SSID_PASSWORD "password"
#define MQTT_SERVER "openhab"
#define MQTT_PORT 1883
#define MQTT_CLIENT_ID "botvac"
#define WIFI_HOSTNAME MQTT_CLIENT_ID
#define MQTT_USER "openhab"
#define MQTT_PASSWORD "password"
#define MQTT_TOPIC_SUBSCRIBE "/home/vacuum/command"
#define MQTT_WILL_TOPIC "/home/vacuum/connected"
#define MQTT_PUB_TOPIC "/home/vacuum/status/"
#define MQTT_RETAIN false

#define NUM_POLL_CMDS 3
String pollCommands[NUM_POLL_CMDS] = {"GetErr", "GetCharger", "GetAnalogSensors"};

#define NUM_ONESHOT_CMDS 3
String oneshotCommands[NUM_ONESHOT_CMDS] = {"GetTime", "GetVersion", "GetWarranty"};

bool queueOneshots = true;

uint8_t curr_pollcmd = 0;
uint8_t curr_oscmd = 0;

// Timer
unsigned long interval = 30000;
unsigned long previousMillis = millis();
bool prioCommandRunning=false;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

//Connect with telnet for debug info
WiFiServer TelnetServer(23);
WiFiClient Telnet;

// Executes a command and waits for a response
void executeSerialCommand(const String& payload, bool isPrioCommand = false) {

  Telnet.println((String)__FUNCTION__ + "(" + payload + ")");

  //consume bytes left on the wire
  while (Serial.available() > 0) {
    Telnet.println((String)"Consuming: " + Serial.read());
  }

  
  Serial.println(payload);
  while (!Serial.available()) { //wait for response
    delay(1);
  }
 

  bool nextIsHeader = false;
  bool parseUnits = false;
  while ((Serial.available())  ) {
    if ((!isPrioCommand) && (prioCommandRunning))
      return;
    yield();
    ArduinoOTA.handle(); //Safety measure in case this loop locks up
    String msg = Serial.readStringUntil('\n');
    //Telnet.println(msg);

    msg.trim();
    
    //remove trailing commas and ^Z
    while ((msg.length() > 0) && ( (msg.lastIndexOf(',') == msg.length() - 1) || (msg.lastIndexOf('\x1A') == msg.length() - 1))) {
      msg = msg.substring(0, msg.length() - 1);
    }

    Telnet.println(msg);
    
    if (msg.indexOf(payload) != -1){
        nextIsHeader = true;
    } else if (nextIsHeader) {
      nextIsHeader = false;
      parseUnits = (msg.indexOf("Unit") != -1);

      if (payload == "GetErr") {
        mqttClient.publish(((String)MQTT_PUB_TOPIC + "error").c_str(), msg.c_str(), MQTT_RETAIN);
      } 
      else if (payload == "GetTime") {
        mqttClient.publish(((String)MQTT_PUB_TOPIC + "time").c_str(), msg.c_str(), MQTT_RETAIN);
      }
      //Telnet.println((String) msg + ": " + (bool)parseUnits);

    } else if ((msg.length() > 0) && (msg.charAt(0) != '\x1A')) {
      //Telnet.println(msg);


      {

        String subtopic, value;
    
        if (parseUnits) {
          uint8_t firstComma = msg.indexOf(',');
          uint8_t secondComma = msg.indexOf(',', firstComma + 1);
          uint8_t lastComma = msg.lastIndexOf(',');
          subtopic = msg.substring(0, firstComma);
          subtopic.trim();
          String unit = msg.substring(firstComma + 1, secondComma);
          unit.trim();
          
          if (secondComma == lastComma){
            value = msg.substring(secondComma + 1);
          } else {
            value = msg.substring(secondComma + 1, lastComma);
          }
          value.trim();
          
          
          subtopic += "_" + unit;
        } else {
          subtopic = msg.substring(0, msg.indexOf(','));
          subtopic.trim();
          value = msg.substring(msg.indexOf(',')+1);
          value.trim();
        }
        //Telnet.println(subtopic + ": " + value);
        mqttClient.publish(((String)MQTT_PUB_TOPIC + subtopic).c_str(), value.c_str(), MQTT_RETAIN);
      }
    }
  }

  previousMillis = millis();
}

void pollVacuum() {

  if (queueOneshots) {
    executeSerialCommand(oneshotCommands[curr_oscmd]);
    if (curr_oscmd + 1 < NUM_ONESHOT_CMDS) {
      curr_oscmd++;
    } else {
      curr_oscmd=0;
      queueOneshots = false;
    }
  } else {
    executeSerialCommand(pollCommands[curr_pollcmd]);
    if (curr_pollcmd + 1 < NUM_POLL_CMDS)
      curr_pollcmd++;
    else
      curr_pollcmd=0;
  }
}

void setup() {

  // botvac serial console is 115200 baud, 8 data bits, no parity, one stop bit (8N1)
  Serial.begin(115200);
  Serial.setTimeout(5000);
  
  setup_wifi();
  ArduinoOTA.begin();
  TelnetServer.begin();
  TelnetServer.setNoDelay(true);

  Telnet.println("setup");
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);

  while (Serial.available() > 0) {
    Serial.read();
  }
  previousMillis = millis();
}

void setup_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.hostname(WIFI_HOSTNAME);
  WiFi.begin(SSID, SSID_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}


void handleTelnet() {
    if (TelnetServer.hasClient()) {
      if (!Telnet || !Telnet.connected()) {
        if (Telnet) Telnet.stop();
        Telnet = TelnetServer.available();
      } else {
        TelnetServer.available().stop();
      }
    }
}

void mqttCallback(char* cptr_topic, byte* bptr_payload, unsigned int payloadLength) {
  String topic(cptr_topic);

  //Some add-a-null-termination-trickery
  char cptr_payload[payloadLength + 1];
  strncpy(cptr_payload, (char*)bptr_payload, payloadLength);
  cptr_payload[payloadLength] = '\0';

  String payload(cptr_payload);

  Telnet.println(((String)__FUNCTION__)+"(" + topic +", " + payload + ", " + payloadLength + ")");
  
  if (topic.length() > 0) {
    prioCommandRunning = true;
    delay(100);
    executeSerialCommand(payload, true);
    prioCommandRunning = false;

  }
}

void mqttReconnect() {
  unsigned int retries = 0;

  while (!mqttClient.connected() && retries < 5) {
    yield();
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD, MQTT_WILL_TOPIC, 0, true, "0")) {
      mqttClient.subscribe(MQTT_TOPIC_SUBSCRIBE);
      mqttClient.publish(MQTT_WILL_TOPIC, "2", true);
      
      queueOneshots = true;
    } else {
      ArduinoOTA.handle();
      retries++;
      delay(5000);
    }
  }
}

void loop() {

  ArduinoOTA.handle();
  handleTelnet();

  if (WiFi.status() != WL_CONNECTED){
    setup_wifi();
  }

  if (!mqttClient.connected()) {
    mqttReconnect();
    Telnet.println("Reconnect MQTT");
  }

  yield();

  if (mqttClient.connected()) {
    mqttClient.loop();
    if (millis() - previousMillis > interval)
      pollVacuum();
  }
}



