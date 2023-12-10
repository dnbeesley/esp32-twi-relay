#include <Arduino.h>
#include <EspMQTTClient.h>
#include <Wire.h>
#include "config.h"

EspMQTTClient client;
unsigned long lastReadTime = 0;
std::vector<ReadDevice> readDevice;
unsigned short readInterval;
size_t outputTopicPrefixLength;
String *ipAdress;
String *password;
String *prefix;
String *topicPrefix;
String *username;
String *wifiPassword;
String *wifiSsid;
SemaphoreHandle_t xMutex;

void setup()
{
#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_ERROR
    Serial.begin(115200);
#endif
    log_d("Initialising config document");
    DynamicJsonDocument config(2048);
    loadConfig(config);

    ipAdress = new String((const char *)config["server"]["ipAddress"]);
    password = new String((const char *)config["auth"]["password"]);
    String prefix = config["topicPrefix"];
    readInterval = config["readInterval"];
    username = new String((const char *)config["auth"]["username"]);
    wifiPassword = new String((const char *)config["wifi"]["password"]);
    wifiSsid = new String((const char *)config["wifi"]["ssid"]);
    short port = config["server"]["port"];

    if (prefix.endsWith("/"))
    {
        topicPrefix = new String(prefix);
    }
    else
    {
        topicPrefix = new String(prefix + "/");
    }

    log_d("Topic prefix: %s", topicPrefix->c_str());

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

    log_d("Connecting to WIFI SSID: %s", wifiSsid->c_str());
    client.setWifiCredentials(wifiSsid->c_str(), wifiPassword->c_str());

    log_d("Client name: %s", username->c_str());
    client.setMqttClientName(username->c_str());

    log_d("Connecting to the MQTT server: %s on port: %d", ipAdress->c_str(), port);
    client.setMqttServer(ipAdress->c_str(), username->c_str(), password->c_str(), port);
    log_i("Connected to the MQTT server");

    Wire.begin();
    xMutex = xSemaphoreCreateMutex();
    log_d("Setup complete");
}

void loop()
{
    int i, j;

    client.loop();
    if (!client.isConnected())
    {
        return;
    }

    if ((millis() - lastReadTime) < 1000 * readInterval)
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
        DynamicJsonDocument doc(1024);
        String output;
        String topic = *topicPrefix + "input/" + String(readDevice[i].address, HEX);
        log_d("Reading from %x", readDevice[i].address);
        Wire.requestFrom(readDevice[i].address, readDevice[i].length);
        for (j = 0; j < readDevice[i].length && Wire.available(); j++)
        {
            doc[j] = Wire.read();
        }

        log_d("Read %d out of %d bytes", j, readDevice[i].length);

        for (; j < readDevice[i].length; j++)
        {
            doc[j] = 0;
        }

        while (Wire.read() != -1)
        {
        }

        serializeJson(doc, output);
        log_d("Writing '%s' to topic: %s", output.c_str(), topic.c_str());
        client.publish(topic, output);
    }
    xSemaphoreGive(xMutex);
}

void onConnectionEstablished()
{
    String topicPattern = *topicPrefix + "output/#";
    outputTopicPrefixLength = topicPattern.length() - 1;
    log_d("Adding subscription to topic: %s", topicPattern.c_str());
    client.subscribe(topicPattern, onReceive, 0);
}

void onReceive(const String &topicStr, const String &message)
{
    String addressStr = topicStr.substring(outputTopicPrefixLength);
    unsigned char address = strtoul(addressStr.c_str(), 0, 16);
    if (address == 0)
    {
        log_e("Invalid address in topic: %s", topicStr.c_str());
    }

    StaticJsonDocument<64> doc;
    size_t i;

    deserializeJson(doc, message);
    JsonArray output = doc.as<JsonArray>();
    if (output.size() == 0)
    {
        log_e("Invalid content in JSON");
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
