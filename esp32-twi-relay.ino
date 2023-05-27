#include <Arduino.h>
#include <ArduinoJson.h>
#include <EspMQTTClient.h>
#include <Wire.h>
#include "config.h"

#define DETECTOR_LENGTH 2
#define IR_TRANSMITTER D8
#define INTERVAL 100000
#define MOTOR_CURRENT_STATE_LENGTH 2
#define OUTPUT_SIZE 64

const uint8_t detectorPins[DETECTOR_LENGTH] = {D9, D10};

EspMQTTClient client(
    WIFI_SSID,
    WIFI_PASSWORD,
    AMQP_IP,
    AMQP_USERNAME,
    AMQP_PASSWORD,
    DEVICE_NAME,
    AMQP_PORT);

hw_timer_t *Timer0_Cfg = NULL;
std::vector<unsigned long> irDetected = {0, 0};
std::vector<unsigned long> irSent = {0, 0};
unsigned long motorSent;
SemaphoreHandle_t xMutex;

void setup()
{
    size_t i;
    Wire.begin();

    for (i = 0; i < DETECTOR_LENGTH; i++)
    {
        pinMode(detectorPins[i], INPUT);
        attachInterrupt(digitalPinToInterrupt(detectorPins[i]), onIrDetect, FALLING);
    }

    xMutex = xSemaphoreCreateMutex();
}

void loop()
{
    StaticJsonDocument<OUTPUT_SIZE> doc;
    String output;
    size_t i;
    client.loop();
    unsigned long start = micros() - INTERVAL;
    if (*std::max_element(irDetected.begin(), irDetected.end()) < start)
    {
        // Start 38KHz output if neither input has gone low since the start of the interval
        tone(IR_TRANSMITTER, 38000);
    }

    if (*std::min_element(irDetected.begin(), irDetected.end()) >= start)
    {
        // Stop 38KHz output if both inputs have gone low since the start of the interval
        noTone(IR_TRANSMITTER);
    }

    for (i = 0; i < DETECTOR_LENGTH; i++)
    {
        // Send event if pin has been high since start of the interval
        if (irDetected[i] < start && irSent[i] < start)
        {
            irSent[i] = micros();
            output = String(i);
            client.publish(AMQP_DETECTOR_PUBLISH_TOPIC, output);
        }
    }

    if (motorSent < start)
    {
        doc.to<JsonArray>();
        if (xSemaphoreTake(xMutex, portMAX_DELAY))
        {
            motorSent = micros();
            Wire.requestFrom(MOTOR_CONTROL_ADDRESS, MOTOR_CURRENT_STATE_LENGTH);
            for (i = 0; i < MOTOR_CURRENT_STATE_LENGTH && Wire.available(); i++)
            {
                doc[i] = Wire.read();
            }

            for (i; i < MOTOR_CURRENT_STATE_LENGTH; i++)
            {
                doc[i] = 0;
            }

            while (Wire.read() != -1)
            {
            }

            xSemaphoreGive(xMutex);

            serializeJson(doc, output);
            client.publish(AMQP_MOTOR_CONTROL_PUBLISH_TOPIC, output);
        }
    }
}

std::function<void(const String &message)> onReceiveFactory(uint8_t address)
{
    return [address](const String &payload)
    {
        StaticJsonDocument<64> doc;
        size_t i;

        deserializeJson(doc, payload);
        JsonArray output = doc.as<JsonArray>();
        if (output.size() == 0)
        {
            return;
        }

        while (!xSemaphoreTake(xMutex, portMAX_DELAY))
        {
        }

        Wire.beginTransmission(address);
        for (i = 0; i < output.size(); i++)
        {
            Wire.write((uint8_t)output[i]);
        }

        Wire.endTransmission();
        xSemaphoreGive(xMutex);
    };
}

void onConnectionEstablished()
{
    client.subscribe(AMQP_MOTOR_CONTROL_SUBSCRIBE_TOPIC, onReceiveFactory(MOTOR_CONTROL_ADDRESS));
    client.subscribe(AMQP_POINTS_CONTROL_SUBSCRIBE_TOPIC, onReceiveFactory(POINTS_CONTROL_ADDRESS));
}

void onIrDetect()
{
    size_t i;
    for (i = 0; i < DETECTOR_LENGTH; i++)
    {
        if (!digitalRead(detectorPins[i]))
        {
            irDetected[i] = micros();
        }
    }
}
