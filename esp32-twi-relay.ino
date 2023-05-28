#include <Arduino.h>
#include <ArduinoJson.h>
#include <EspMQTTClient.h>
#include <Wire.h>
#include "config.h"

#define CHECK_INTERVAL 100000
#define DETECTOR_LENGTH 2
#define IR_TRANSMITTER D8
#define MOTOR_CURRENT_STATE_LENGTH 2
#define OUTPUT_SIZE 64
#define PRESCALER (F_CPU / 1000000UL)
#define PULSE_FREQUENCY 38000
#define PULSE_INTERVAL 5000
#define PULSE_LENGTH 1000

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
unsigned long irDetected[] = {ULONG_MAX, ULONG_MAX};
unsigned long irSent[] = {0, 0};
unsigned long motorSent = 0;
hw_timer_t *pulseTimer;
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

    pulseTimer = timerBegin(1, PRESCALER, true);
    timerAttachInterrupt(pulseTimer, onStartPulse, true);
    timerAlarmWrite(pulseTimer, PULSE_INTERVAL, true);
    timerAlarmEnable(pulseTimer);
    log_d("Setup complete");
}

void loop()
{
    StaticJsonDocument<OUTPUT_SIZE> doc;
    String output;
    size_t i;
    client.loop();
    unsigned long start = micros() - CHECK_INTERVAL;

    for (i = 0; i < DETECTOR_LENGTH; i++)
    {
        // Send event if pin has been high since start of the interval
        if (irDetected[i] < start && irSent[i] < start)
        {
            log_d("IR beam on input: %d was last detected at: %d and a message sent at: %d", i, irDetected[i], irSent[i]);
            irSent[i] = micros();
            output = String(i);
            log_d("Sending new message");
            client.publish(AMQP_DETECTOR_PUBLISH_TOPIC, output);
        }
    }

    if (motorSent < start)
    {
        doc.to<JsonArray>();
        if (xSemaphoreTake(xMutex, portMAX_DELAY))
        {
            motorSent = micros();
            log_d("Reading from %x", MOTOR_CONTROL_ADDRESS);
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
            log_d("Writing '%s' to topic: %s", output.c_str(), AMQP_MOTOR_CONTROL_PUBLISH_TOPIC);
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

void onStartPulse()
{
    tone(IR_TRANSMITTER, PULSE_FREQUENCY, PULSE_LENGTH);
}
