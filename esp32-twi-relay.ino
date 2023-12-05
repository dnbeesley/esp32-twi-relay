#include <Arduino.h>
#include <ArduinoJson.h>
#include <EspMQTTClient.h>
#include <Wire.h>
#include "config.h"

EspMQTTClient client;
unsigned long lastReadTime;
std::vector<ReadDevice> readDevice;
unsigned short readInterval;
size_t outputTopicPrefixLength;
String topicPrefix;
SemaphoreHandle_t xMutex;

void setup()
{
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_ERROR
    Serial.begin(115200);
#endif
    log_d("Initialising config document");
    DynamicJsonDocument config(2048);
    loadConfig(config);

    String detectorTopic = config["detectorTopic"];
    String ipAdress = config["server"]["ipAddress"];
    String password = config["auth"]["password"];
    String prefix = config["topicPrefix"];
    readInterval = config["readInterval"];
    String username = config["auth"]["username"];
    String wifiPassword = config["wifi"]["password"];
    String wifiSsid = config["wifi"]["ssid"];
    short port = config["server"]["port"];

    if (prefix.endsWith("/"))
    {
        topicPrefix = prefix;
    }
    else
    {
        topicPrefix = prefix + "/";
    }

    JsonArray array = config["readDevices"];
    readDevice = std::vector<ReadDevice>(array.size());
    for (int i = 0; i < array.size(); i++)
    {
        readDevice[i].address = array[i]["address"];
        readDevice[i].length = array[i]["length"];
    }

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
    client.enableDebuggingMessages();
#endif

    log_d("Connecting to WIFI SSID: %s", config.wifiSsid.c_str());
    client.setWifiCredentials(wifiSsid.c_str(), wifiPassword.c_str());

    log_d("Client name: %s", config.mqttUsername.c_str());
    client.setMqttClientName(username.c_str());

    log_d("Connecting to the MQTT server: %s on port: %d", mqttServerIp.c_str(), port);
    client.setMqttServer(ipAdress.c_str(), username.c_str(), password.c_str(), port);
    log_i("Connected to the MQTT server");

    log_d("Setup complete");
}

void loop()
{
    int i, j;

    client.loop();
    if ((lastReadTime - millis()) < 1000 * readInterval)
    {
        return;
    }

    lastReadTime = millis();
    while (!xSemaphoreTake(xMutex, portMAX_DELAY))
    {
        log_d("Waiting for lock on mutex");
    }

    for (i = 0; i < readDevice.size(); i++)
    {
        JsonArray bytes;
        String output;
        String topic = topicPrefix + "input/" + String(readDevice[i].address, HEX);
        log_d("Reading from %x", readDevice[i].address);
        Wire.requestFrom(readDevice[i].address, readDevice[i].length);
        for (j = 0; j < readDevice[i].length && Wire.available(); j++)
        {
            bytes[i] = Wire.read();
        }

        for (; j < readDevice[i].length; j++)
        {
            bytes[i] = 0;
        }

        while (Wire.read() != -1)
        {
        }

        serializeJson(bytes, output);
        log_d("Writing '%s' to topic: %s", output.c_str(), topic);
        client.publish(topic, output);
    }
    xSemaphoreGive(xMutex);
}

void onConnectionEstablished()
{
    String topicPattern;
    topicPattern += "output/#";
    outputTopicPrefixLength = topicPattern.length() - 1;
    log_d("Adding subscription to topic: %s", topicPattern.c_str());
    client.subscribe(topicPrefix, onReceive, 0);
}

void onReceive(const String &topicStr, const String &message)
{
    String addressStr = topicStr.substring(outputTopicPrefixLength);
    unsigned char address = strtoul(addressStr.c_str(), 0, 16);
    if (address == 0)
    {
        log_e("Invalid addrss in topic: %s", topicStr.c_str());
    }

    StaticJsonDocument<64> doc;
    size_t i;

    deserializeJson(doc, message);
    JsonArray output = doc.as<JsonArray>();
    if (output.size() == 0)
    {
        log_e("Invalid conetent in JSON");
        return;
    }

    while (!xSemaphoreTake(xMutex, portMAX_DELAY))
    {
        log_d("Waiting for lock on mutex");
    }

    log_d("Writing %d bytes state to: %x", output.size(), address);
    Wire.beginTransmission(address);
    for (i = 0; i < output.size(); i++)
    {
        Wire.write((uint8_t)output[i]);
    }

    Wire.endTransmission();
    xSemaphoreGive(xMutex);
}
