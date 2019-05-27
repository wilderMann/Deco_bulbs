#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <DHTesp.h>
#include <Ticker.h>
#include "config.h"
#include "homie.hpp"
#include "LEDString.hpp"

#define SERIAL true
#define CLIENTID "ESP8266Client-"
#define LAMPS D2
#define DHT D5

#define MEASSURETIME 300

const char *update_path = "/firmware";
const char *update_username = USERNAME;
const char *update_password = PASSWORD;

typedef struct data_s {
        float temp;
        float humidity;
        float heatIndex;
}data_t;


void httpServer_ini();
void callback(char *topic, byte *payload, unsigned int length);
boolean reconnect();
void lampMove(int duty, int time = 0);
void lampSet(int duty);
void lampOnOff(bool onoff);
void lampMQTTUpdate(int time = 1000);
void checkState();
data_t meassurement();
void funWithFlags();
void meassureHandler();
void heartBeatHandler();


string ClientID;
unsigned long lastReconnectAttempt = 0;
uint8_t flag_measure;
uint8_t flag_heartBeat;

ESP8266HTTPUpdateServer httpUpdater;
ESP8266WebServer httpServer(80);
WiFiClient espClient;
PubSubClient client(MQTT_IP, MQTT_PORT, callback, espClient);
Homie homieCTRL = Homie(&client);
LEDString lamps(LAMPS);
Ticker wait;
DHTesp dht22;
Ticker dhtMeasure;
Ticker heartBeat;

void setup() {
        Serial.begin(115200);
        while (!Serial);
        WiFi.mode(WIFI_STA);
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        while (WiFi.status() != WL_CONNECTED) {
                delay(500);
                if(SERIAL) Serial.print(".");
        }
        if(SERIAL) Serial.println("");
        if(SERIAL) Serial.print("Connected, IP address: ");
        if(SERIAL) Serial.println(WiFi.localIP());

        httpServer_ini();
        lamps.init();
        dht22.setup(D5, DHTesp::DHT22);
        ClientID = string(CLIENTID) + DEVICE_NAME;

        HomieDevice homieDevice = HomieDevice(DEVICE_NAME, "Deco Bulbs", WiFi.localIP().toString().c_str(),
                                              WiFi.macAddress().c_str(), FW_NAME, FW_VERSION,
                                              "esp8266", "60");

        HomieNode lights = HomieNode("lights", "Lights", "LEDDimmer");
        HomieNode dht = HomieNode("air-sensors", "Air Sensors", "DHT22");
        HomieProperties brightness = HomieProperties("brightness", "Brightness",
                                                     true, true, "%",
                                                     homie::float_t, "0:1");
        HomieProperties power = HomieProperties("power", "Power",
                                                true, true, "",
                                                homie::boolean_t, "");
        HomieProperties temperature = HomieProperties("temperature", "Temperature",
                                                      false, true, "°C",
                                                      homie::float_t);
        HomieProperties humidity = HomieProperties("humidity", "Humidity", false,
                                                   true, "%", homie::float_t);
        HomieProperties heatIndex = HomieProperties("heat-index", "heat index",
                                                    false, true, "°C",
                                                    homie::float_t);
        dht.addProp(temperature);
        dht.addProp(humidity);
        dht.addProp(heatIndex);
        lights.addProp(brightness);
        lights.addProp(power);
        homieDevice.addNode(dht);
        homieDevice.addNode(lights);
        homieCTRL.setDevice(homieDevice);

        homieCTRL.connect(ClientID.c_str(), MQTT_USR, MQTT_PW);
        dhtMeasure.attach(MEASSURETIME, meassureHandler);
        heartBeat.attach(60.0, heartBeatHandler);

        lamps.set(0);
        delay(500);
        lamps.set(100);
        delay(500);
        lampMove(0);
        delay(500);
}

void loop() {
        if (!homieCTRL.connected()) {
                unsigned long now = millis();
                if (now - lastReconnectAttempt > 5000) {
                        lastReconnectAttempt = now;
                        // Attempt to reconnect
                        if (reconnect()) {
                                lastReconnectAttempt = 0;
                        }
                }
        }
        homieCTRL.loop();
        httpServer.handleClient();
        funWithFlags();
        MDNS.update();
}

boolean reconnect() {
        // Loop until we're reconnected
        return homieCTRL.connect(ClientID.c_str(), MQTT_USR, MQTT_PW);
}

void httpServer_ini() {
        char buffer[100];
        sprintf(buffer, "%s", DEVICE_NAME);
        MDNS.begin(buffer);
        httpUpdater.setup(&httpServer, update_path, update_username, update_password);
        httpServer.begin();
        MDNS.addService("http", "tcp", 80);
        if(SERIAL) Serial.printf("HTTPUpdateServer ready! Open http://%s.local%s in your "
                                 "browser and login with username '%s' and password '%s'\n",
                                 buffer, update_path, update_username, update_password);
        //------
}

data_t meassurement(){
        data_t data;
        data.temp= dht22.getTemperature();
        data.humidity = dht22.getHumidity();
        data.heatIndex = dht22.computeHeatIndex(data.temp, data.humidity, false);
        return data;
}

void funWithFlags(){
        if(flag_measure) {
                data_t data = meassurement();
                char buffer[6]; //3 char + point + 2 char
                sprintf(buffer, "%.2f", data.temp);
                string topic = "homie/" + string(DEVICE_NAME) + "/air-sensors/temperature";
                client.publish(topic.c_str(),buffer,true);
                sprintf(buffer, "%.2f", data.humidity);
                topic = "homie/" + string(DEVICE_NAME) + "/air-sensors/humidity";
                client.publish(topic.c_str(),buffer,true);
                sprintf(buffer, "%.2f", data.heatIndex);
                topic = "homie/" + string(DEVICE_NAME) + "/air-sensors/heat-index";
                client.publish(topic.c_str(),buffer,true);
                flag_measure = 0;
        }
        if(flag_heartBeat) {
                long time = millis() / 1000;
                string topic = "homie/" + string(DEVICE_NAME) + "/$stats/uptime";
                char payload[20];
                sprintf(payload, "%ld", time);
                client.publish(topic.c_str(), payload,true);
                topic = "homie/" + string(DEVICE_NAME) + "/$stats/interval";
                client.publish(topic.c_str(), "60",true);
                flag_heartBeat = 0;
        }
}

void callback(char *topic, byte *payload, unsigned int length){
        string topicString = string(topic);
        if(SERIAL) Serial.println(topicString.c_str());

        std::size_t found = topicString.find("lights/brightness/set");
        if(found!=std::string::npos) {
                char buffer[10];
                snprintf(buffer, length + 1, "%s", payload);
                //int duty = atoi(buffer);
                double duty_f = atof(buffer);
                lampMove((int)(duty_f*100.0));
        }

        found = topicString.find("lights/power/set");
        if(found!=std::string::npos) {
                char buffer[6];
                snprintf(buffer, length + 1, "%s", payload);
                if (!strcmp(buffer, "true")) {
                        lampOnOff(true);
                        return;
                }
                if (!strcmp(buffer, "false")) {
                        lampOnOff(false);
                        return;
                }
        }
}

void lampMove(int duty, int time){
        if(time == 0) {
                lamps.move(duty);
                Serial.println("move");
        }else{
                lamps.move(duty,time);
                Serial.println("move time");
        }
        lampMQTTUpdate();
}
void lampSet(int duty){
        lamps.set(duty);
        lampMQTTUpdate();
}
void lampOnOff(bool onoff){
        if(onoff) {
                lamps.on();
        }else{
                lamps.off();
        }
        lampMQTTUpdate();
}

void lampMQTTUpdate(int time){
        if(homieCTRL.connected()) {
                if(lamps.ismoving()) {
                        wait.once_ms(100, checkState);
                }else{
                        char newDuty[6];
                        float duty = (float) lamps.getDuty() / 100.0;
                        //itoa(lamps.getDuty(),newDuty,10); //Bug in openHab, workaround for an object Dimmer
                        sprintf(newDuty,"%.3f",duty);
                        string power = "false";
                        if(lamps.getStatus()) {
                                power = "true";
                        }
                        string feedbackTopic = homieCTRL.getPubString("lights", "brightness");
                        client.publish(feedbackTopic.c_str(),newDuty,true);
                        feedbackTopic = homieCTRL.getPubString("lights", "power");
                        client.publish(feedbackTopic.c_str(),power.c_str(),true);
                }

        }
}

void checkState(){
        lampMQTTUpdate();
}

void meassureHandler(){
        flag_measure = 1;
}

void heartBeatHandler(){
        flag_heartBeat = 1;
}
